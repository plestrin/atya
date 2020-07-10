#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>

#include "index.h"
#include "simple_index.h"
#include "gory_sewer.h"
#include "utile.h"

static uint8_t chunk[0x10000];

static int insert_file(struct index_root* ir, const char* file_name){
	FILE* handle;
	int status;
	size_t size;

	if ((handle = fopen(file_name, "rb")) == NULL){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, file_name);
		return status;
	}

	size = fread(chunk, 1, sizeof chunk, handle);
	for ( ; ; ){
		if ((status = index_root_do_insert_buffer(ir, chunk, size))){
			fprintf(stderr, "[-] in %s, unable to index buffer in %s\n", __func__, file_name);
			break;
		}
		memmove(chunk, chunk + size - ir->size + 1, ir->size - 1);
		size = fread(chunk + ir->size - 1, 1, sizeof chunk - ir->size + 1, handle);
		if (!size){
			break;
		}
		size += ir->size - 1;
	}

	fclose(handle);

	return status;
}

static int root_compare_file(struct index_root* ir, const char* file_name){
	FILE* handle;
	int status;
	size_t rd_sz;

	if ((handle = fopen(file_name, "rb")) == NULL){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, file_name);
		return status;
	}

	rd_sz = fread(chunk, 1, sizeof chunk, handle);
	for ( ; ; ){
		index_root_do_compare_buffer(ir, chunk, rd_sz);
		memmove(chunk, chunk + rd_sz - ir->size + 1, ir->size - 1);
		rd_sz = fread(chunk + ir->size - 1, 1, (sizeof chunk) - ir->size + 1, handle);
		if (!rd_sz){
			break;
		}
		rd_sz += ir->size - 1;
	}

	fclose(handle);

	return 0;
}

static int simple_compare_file(struct simple_index* si, const char* file_name, uint64_t* cnt_ptr){
	FILE* handle;
	int status;
	size_t rd_sz;
	uint64_t cnt;

	if ((handle = fopen(file_name, "rb")) == NULL){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, file_name);
		return status;
	}

	rd_sz = fread(chunk, 1, sizeof chunk, handle);
	for (cnt = 0; ; ){
		cnt = simple_index_compare_buffer(si, chunk, rd_sz);
		memmove(chunk, chunk + rd_sz - si->size + 1, si->size - 1);
		rd_sz = fread(chunk + si->size - 1, 1, (sizeof chunk) - si->size + 1, handle);
		if (!rd_sz){
			break;
		}
		rd_sz += si->size - 1;
	}

	fclose(handle);

	if (cnt_ptr != NULL){
		*cnt_ptr = cnt;
	}

	return 0;
}

static int root_intersect_files(struct index_root* ir, struct gory_sewer_knob* gsk_files){
	char* file_path;
	int status = 0;

	for (file_path = gory_sewer_first(gsk_files); file_path != NULL; file_path = gory_sewer_next(gsk_files)){
		if ((status = root_compare_file(ir, file_path))){
			fprintf(stderr, "[-] in %s, unable to compare to file: %s\n", __func__, file_path);
			break;
		}
		index_root_do_intersect(ir);
	}

	return status;
}

static int simple_intersect_files(struct simple_index* si, struct gory_sewer_knob* gsk_files){
	char* file_path;
	int status = 0;

	for (file_path = gory_sewer_first(gsk_files); file_path != NULL; file_path = gory_sewer_next(gsk_files)){
		if ((status = simple_compare_file(si, file_path, NULL))){
			fprintf(stderr, "[-] in %s, unable to compare to file: %s\n", __func__, file_path);
			break;
		}
	}
	simple_index_remove_nohit(si);

	return status;
}

#define EXCLUDE_STEP 64

static int root_exclude_files(struct index_root* ir, struct gory_sewer_knob* gsk_files){
	char* file_path;
	int status = 0;
	uint64_t i;

	for (file_path = gory_sewer_first(gsk_files), i = 0; file_path != NULL; file_path = gory_sewer_next(gsk_files)){
		if ((status = root_compare_file(ir, file_path))){
			fprintf(stderr, "[-] in %s, unable to compare to file: %s\n", __func__, file_path);
			break;
		}
		if (++i % EXCLUDE_STEP){
			if (index_root_do_exclude(ir)){
				return 0;
			}
		}
	}
	if (!(i % EXCLUDE_STEP)){
		index_root_do_exclude(ir);
	}

	return status;
}

static int simple_exclude_files(struct simple_index* si, struct gory_sewer_knob* gsk_files){
	char* file_path;
	int status = 0;
	uint64_t cnt;

	for (file_path = gory_sewer_first(gsk_files); file_path != NULL; file_path = gory_sewer_next(gsk_files)){
		if (!file_path[0]){
			continue;
		}

		if ((status = simple_compare_file(si, file_path, &cnt))){
			fprintf(stderr, "[-] in %s, unable to compare to file: %s\n", __func__, file_path);
		}
		else if (!cnt){
			file_path[0] = 0;
		}
	}
	simple_index_remove_hit(si);

	return status;
}

static int index_root_create(struct gory_sewer_knob* gsk_files, size_t index_size, struct index_root** ir_ptr){
	struct index_root* ir;
	int status;
	char* first_file_path;

	if ((ir = calloc(sizeof(struct index_root), 1)) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}
	ir->size = index_size;

	*ir_ptr = ir;
	if ((first_file_path = gory_sewer_first(gsk_files)) == NULL){
		return 0;
	}

	if ((status = insert_file(ir, first_file_path))){
		index_root_do_clean(ir);
	}
	else {
		if ((status = root_intersect_files(ir, gsk_files))){
			index_root_do_clean(ir);
		}
	}

	return status;
}

static int index_root_next(struct index_root* ir, struct gory_sewer_knob* gsk_files, struct index_root** ir_next_ptr){
	uint8_t* value;
	struct index_root* ir_next;
	int cont1;
	int cont2;
	int status;

	value = alloca(ir->size + 1);

	ir_next = *ir_next_ptr;
	if (ir_next == NULL){
		ir_next = calloc(sizeof(struct index_root), 1);
		*ir_next_ptr = ir_next;
	}

	if (ir_next == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}

	ir_next->size = ir->size + 1;

	for (cont1 = index_root_do_get_first(ir, value, 0); cont1; cont1 = index_root_do_get_next(ir, value, 0)){
		for (cont2 = index_root_do_get_first(ir, value + 1, ir->size - 1); cont2; cont2 = index_root_do_get_next(ir, value + 1, ir->size -1)){
			if ((status = index_root_do_insert(ir_next, value))){
				index_root_do_clean(ir_next);
				return status;
			}
		}
	}

	if ((status = root_intersect_files(ir_next, gsk_files))){
		index_root_do_clean(ir_next);
		return status;
	}

	for (cont1 = index_root_do_get_first(ir_next, value, 0); cont1; cont1 = index_root_do_get_next(ir_next, value, 0)){
		index_root_do_compare(ir, value);
		index_root_do_compare(ir, value + 1);
	}

	index_root_do_exclude(ir);

	return 0;
}

static int index_root_next_mix(struct index_root* ir, struct gory_sewer_knob* gsk_files, struct simple_index** si_next_ptr){
	uint8_t* value;
	uint64_t iter;
	struct simple_index* si_next;
	int cont1;
	int cont2;
	int status;

	value = alloca(ir->size + 1);

	si_next = *si_next_ptr;
	if (si_next == NULL){
		if ((status = simple_index_create(&si_next, ir->size + 1))){
			fprintf(stderr, "[-] in %s, unable to create simple index\n", __func__);
			return status;
		}
		*si_next_ptr = si_next;
	}

	si_next->size = ir->size + 1;

	for (cont1 = index_root_do_get_first(ir, value, 0); cont1; cont1 = index_root_do_get_next(ir, value, 0)){
		for (cont2 = index_root_do_get_first(ir, value + 1, ir->size - 1); cont2; cont2 = index_root_do_get_next(ir, value + 1, ir->size -1)){
			if ((status = simple_index_insert(si_next, value))){
				return status;
			}
		}
	}

	if ((status = simple_intersect_files(si_next, gsk_files))){
		return status;
	}

	for (iter = 0; simple_index_get(si_next, value, &iter); iter ++){
		index_root_do_compare(ir, value);
		index_root_do_compare(ir, value + 1);
	}

	index_root_do_exclude(ir);

	return 0;
}

static int index_simple_next(struct simple_index* si, struct gory_sewer_knob* gsk_in, struct gory_sewer_knob* gsk_ex, struct simple_index** si_next_ptr){
	uint8_t* value;
	uint64_t iter1;
	uint32_t iter2;
	struct simple_index* si_next;
	uint16_t hash;
	int status;

	value = alloca(si->size + 1);

	si_next = *si_next_ptr;
	if (si_next == NULL){
		if ((status = simple_index_create(&si_next, si->size + 1))){
			fprintf(stderr, "[-] in %s, unable to create simple index\n", __func__);
			return status;
		}
		*si_next_ptr = si_next;
	}

	si_next->size = si->size + 1;

	for (iter1 = 0; simple_index_get(si, value, &iter1); iter1 ++){
		hash = hash_init(value + 1, si->size - 1);
		for (iter2 = 0; simple_index_get_hash(si, value + 1, hash, &iter2); iter2 ++){
			if ((status = simple_index_insert(si_next, value))){
				return status;
			}
		}
	}

	if ((status = simple_intersect_files(si_next, gsk_in))){
		return status;
	}

	for (iter1 = 0; simple_index_get(si_next, value, &iter1); iter1 ++){
		simple_index_compare(si, value);
		simple_index_compare(si, value + 1);
	}

	if (simple_index_count_nohit(si)){
		if (simple_exclude_files(si, gsk_ex)){
			fprintf(stderr, "[-] in %s, unable to exclude files\n", __func__);
		}
	}
	else {
		simple_index_clean(si);
	}

	return 0;
}

#define START 5
#define SIMPLE 12
#define STOP 2048

static char path[4096];

int main(int argc, char** argv){
	int i;
	struct index_root* ir_buffer[2] = {NULL, NULL};
	int ir_index = 0;
	struct simple_index* si_buffer[2] = {NULL, NULL};
	int si_index = 0;
	struct gory_sewer_knob* gsk_in;
	struct gory_sewer_knob* gsk_ex;
	struct gory_sewer_knob* gsk;
	uint64_t nb_pattern;
	int status = EXIT_SUCCESS;

	if (argc < 2){
		fprintf(stderr, "[-] usage: file [...] [-e [...]]\n");
		return EXIT_FAILURE;
	}

	gsk_in = gory_sewer_create(0x4000);
	gsk_ex = gory_sewer_create(0x4000);

	if (gsk_in == NULL || gsk_ex == NULL){
		fprintf(stderr, "[-] in %s, unable to init gory sewers\n", __func__);
		if (gsk_in != NULL){
			gory_sewer_delete(gsk_in);
		}
		if (gsk_ex != NULL){
			gory_sewer_delete(gsk_ex);
		}
		return EXIT_FAILURE;
	}

	for (i = 1, gsk = gsk_in; i < argc; i++){
		if (!strcmp(argv[i], "-e")){
			gsk = gsk_ex;
			continue;
		}
		strncpy(path, argv[i], sizeof path);
		path[sizeof path - 1] = 0;
		if (list_files(path, sizeof path, gsk)){
			fprintf(stderr, "[-] in %s, unable to list files in: %s\n", __func__, path);
		}
	}

	fprintf(stderr, "[+] in %s, parsing command line: %lu inc file(s), %lu exc files(s)\n", __func__, gsk_in->nb_item, gsk_ex->nb_item);

	if (index_root_create(gsk_in, START, ir_buffer)){
		fprintf(stderr, "[-] in %s, unable to create index of size %u\n", __func__, START);
		status = EXIT_FAILURE;
	}
	else {
		nb_pattern = index_root_do_count(ir_buffer[ir_index]);
		fprintf(stderr, "[+] starting with %lu pattern(s) of size %zu in inc file(s)\n", nb_pattern, ir_buffer[ir_index]->size);

		while (nb_pattern && ir_buffer[ir_index]->size <= SIMPLE){
			if (index_root_next(ir_buffer[ir_index], gsk_in, ir_buffer + ((ir_index + 1) & 0x1))){
				fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, ir_buffer[ir_index]->size + 1);
				status = EXIT_FAILURE;
				goto exit;
			}

			if (index_root_do_count(ir_buffer[ir_index])){
				if (root_exclude_files(ir_buffer[ir_index], gsk_ex)){
					fprintf(stderr, "[-] in %s, unable to exclude files\n", __func__);
				}

				index_root_do_dump(ir_buffer[ir_index]);
				index_root_do_clean(ir_buffer[ir_index]);
			}

			ir_index = (ir_index + 1) & 0x1;
			nb_pattern = index_root_do_count(ir_buffer[ir_index]);
		}

		if (!nb_pattern){
			goto exit;
		}

		if (ir_buffer[ir_index]->size > STOP){
			if (root_exclude_files(ir_buffer[ir_index], gsk_ex)){
				fprintf(stderr, "[-] in %s, unable to exclude files\n", __func__);
			}

			index_root_do_dump(ir_buffer[ir_index]);
			goto exit;
		}

		fprintf(stderr, "[+] starting simple with %lu pattern(s) of size %zu in inc file(s)\n", nb_pattern, ir_buffer[ir_index]->size);

		if (index_root_next_mix(ir_buffer[ir_index], gsk_in, si_buffer)){
			fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, ir_buffer[ir_index]->size + 1);
			status = EXIT_FAILURE;
			goto exit;
		}

		if (index_root_do_count(ir_buffer[ir_index])){
			if (root_exclude_files(ir_buffer[ir_index], gsk_ex)){
				fprintf(stderr, "[-] in %s, unable to exclude files\n", __func__);
			}

			index_root_do_dump(ir_buffer[ir_index]);
			index_root_do_clean(ir_buffer[ir_index]);
		}

		while (simple_index_count(si_buffer[si_index]) && si_buffer[si_index]->size <= STOP){
			if (index_simple_next(si_buffer[si_index], gsk_in, gsk_ex, si_buffer + ((si_index + 1) & 0x1))){
				fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, si_buffer[si_index]->size + 1);
				status = EXIT_FAILURE;
				goto exit;
			}

			simple_index_dump(si_buffer[si_index]);

			simple_index_clean(si_buffer[si_index]);
			si_index = (si_index + 1) & 0x1;
		}

		if (simple_exclude_files(si_buffer[si_index], gsk_ex)){
			fprintf(stderr, "[-] in %s, unable to exclude files\n", __func__);
		}
		else {
			simple_index_dump(si_buffer[si_index]);
		}
	}

	exit:
	if (ir_buffer[0] != NULL){
		index_root_do_delete(ir_buffer[0]);
	}
	if (ir_buffer[1] != NULL){
		index_root_do_delete(ir_buffer[1]);
	}

	gory_sewer_delete(gsk_in);
	gory_sewer_delete(gsk_ex);

	if (si_buffer[0] != NULL){
		simple_index_delete(si_buffer[0]);
	}
	if (si_buffer[1] != NULL){
		simple_index_delete(si_buffer[1]);
	}

	return status;
}
