#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>

#include "fast_index.h"
#include "simple_index.h"
#include "last_index.h"
#include "abs_storage.h"
#include "gory_sewer.h"
#include "utile.h"

static int fast_push(struct last_index* li, struct fast_index* fi){
	uint8_t* value;
	int cont;
	uint64_t cnt;
	int status;

	value = alloca(fi->size);

	for (cont = fast_index_get_first(fi, value, 0), cnt = 0; cont; cont = fast_index_get_next(fi, value, 0), cnt ++){
		if ((status = last_index_push(li, value, fi->size))){
			fprintf(stderr, "[-] in %s, unable to push data to last index\n", __func__);
			return status;
		}
	}

	if (cnt){
		fprintf(stderr, "[+] %5lu patterns of size %4zu\n", cnt, fi->size);
	}

	return 0;
}

static int simple_push(struct last_index* li, struct simple_index* si){
	const uint8_t* ptr;
	uint64_t iter;
	uint64_t cnt;
	int status;

	for (iter = 0, cnt = 0; simple_index_get(si, &ptr, &iter); iter ++, cnt ++){
		if ((status = last_index_push(li, ptr, si->size))){
			fprintf(stderr, "[-] in %s, unable to push data to last index\n", __func__);
			return status;
		}
	}

	if (cnt){
		fprintf(stderr, "[+] %5lu patterns of size %4zu\n", cnt, si->size);
	}

	return 0;
}

static int last_exclude_files(struct last_index* li, struct gory_sewer_knob* gsk_files){
	char* file_path;
	int status = 0;

	fprintf(stderr, "[+] starting exclusion with %lu patterns\n", li->nb_item);

	for (file_path = gory_sewer_first(gsk_files); file_path != NULL; file_path = gory_sewer_next(gsk_files)){
		if ((status = last_index_exclude_file(li, file_path))){
			fprintf(stderr, "[-] in %s, unable to exclude file: %s\n", __func__, file_path);
			break;
		}
	}

	return status;
}

static uint8_t chunk[0x10000];

static int fast_insert_file(struct fast_index* fi, const char* file_name){
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
		if ((status = fast_index_insert_buffer(fi, chunk, size))){
			fprintf(stderr, "[-] in %s, unable to index buffer in %s\n", __func__, file_name);
			break;
		}
		memmove(chunk, chunk + size - fi->size + 1, fi->size - 1);
		size = fread(chunk + fi->size - 1, 1, sizeof chunk - fi->size + 1, handle);
		if (!size){
			break;
		}
		size += fi->size - 1;
	}

	fclose(handle);

	return status;
}

static int fast_create(struct gory_sewer_knob* gsk_files, struct abs_storage* as, size_t index_size, struct fast_index** fi_ptr){
	struct fast_index* fi;
	char* file_path;
	int status = 0;

	if ((fi = calloc(sizeof(struct fast_index), 1)) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}

	fi->size = index_size;
	*fi_ptr = fi;

	if ((file_path = gory_sewer_first(gsk_files)) != NULL){
		if ((status = fast_insert_file(fi, file_path))){
			fprintf(stderr, "[-] in %s, unable to insert file %s to fast index\n", __func__, file_path);
		}
		else {
			struct abs_index ai;
			abs_index_init_fast(&ai, fi);
			status = abs_storage_intersect(as, &ai, 1);
		}
	}

	return status;
}

static int fast_next(struct fast_index* fi, struct abs_storage* as, struct fast_index** fi_next_ptr){
	uint8_t* value;
	struct fast_index* fi_next;
	int cont1;
	int cont2;
	int status;

	value = alloca(fi->size + 1);

	fi_next = *fi_next_ptr;
	if (fi_next == NULL){
		fi_next = calloc(sizeof(struct fast_index), 1);
		*fi_next_ptr = fi_next;
	}

	if (fi_next == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}

	fi_next->size = fi->size + 1;

	for (cont1 = fast_index_get_first(fi, value, 0); cont1; cont1 = fast_index_get_next(fi, value, 0)){
		for (cont2 = fast_index_get_first(fi, value + 1, fi->size - 1); cont2; cont2 = fast_index_get_next(fi, value + 1, fi->size -1)){
			if ((status = fast_index_insert(fi_next, value))){
				fast_index_clean(fi_next);
				return status;
			}
		}
	}

	struct abs_index ai_next;
	abs_index_init_fast(&ai_next, fi_next);
	if ((status = abs_storage_intersect(as, &ai_next, 0))){
		return status;
	}

	for (cont1 = fast_index_get_first(fi_next, value, 0); cont1; cont1 = fast_index_get_next(fi_next, value, 0)){
		fast_index_compare(fi, value);
		fast_index_compare(fi, value + 1);
	}

	fast_index_remove_hit(fi);

	return 0;
}

static int mixed_next(struct fast_index* fi, struct abs_storage* as, struct simple_index** si_next_ptr){
	uint8_t* value;
	const uint8_t* ptr;
	uint64_t iter;
	struct simple_index* si_next;
	int cont1;
	int cont2;
	int status;

	value = alloca(fi->size + 1);

	si_next = *si_next_ptr;
	if (si_next == NULL){
		if ((status = simple_index_create(&si_next, fi->size + 1))){
			fprintf(stderr, "[-] in %s, unable to create simple index\n", __func__);
			return status;
		}
		*si_next_ptr = si_next;
	}

	si_next->size = fi->size + 1;

	for (cont1 = fast_index_get_first(fi, value, 0); cont1; cont1 = fast_index_get_next(fi, value, 0)){
		for (cont2 = fast_index_get_first(fi, value + 1, fi->size - 1); cont2; cont2 = fast_index_get_next(fi, value + 1, fi->size -1)){
			if ((status = simple_index_insert(si_next, value))){
				return status;
			}
		}
	}

	struct abs_index ai_next;
	abs_index_init_simple(&ai_next, si_next);
	if ((status = abs_storage_intersect(as, &ai_next, 0))){
		return status;
	}

	for (iter = 0; simple_index_get(si_next, &ptr, &iter); iter ++){
		fast_index_compare(fi, ptr);
		fast_index_compare(fi, ptr + 1);
	}

	fast_index_remove_hit(fi);

	return 0;
}

static int simple_next(struct simple_index* si, struct abs_storage* as, struct simple_index** si_next_ptr){
	uint8_t* value;
	const uint8_t* ptr;
	uint64_t iter1;
	uint32_t iter2;
	struct simple_index* si_next;
	uint16_t hash1;
	uint16_t hash2;
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

	for (iter1 = 0; simple_index_get_cpy(si, value, &iter1); iter1 ++){
		hash1 = simple_index_hash_next(si, simple_index_iter_get_hash(iter1), value);
		for (iter2 = 0; simple_index_get_hash(si, value + 1, hash1, &iter2); iter2 ++){
			hash2 = simple_index_hash_increase(si, simple_index_iter_get_hash(iter1), value);
			if ((status = simple_index_insert_hash(si_next, value, hash2))){
				return status;
			}
		}
	}

	struct abs_index ai_next;
	abs_index_init_simple(&ai_next, si_next);
	if ((status = abs_storage_intersect(as, &ai_next, 0))){
		return status;
	}

	for (iter1 = 0; simple_index_get(si_next, &ptr, &iter1); iter1 ++){
		hash1 = simple_index_hash_decrease(si_next, simple_index_iter_get_hash(iter1), ptr);
		simple_index_compare_hash(si, ptr, hash1);
		hash1 = simple_index_hash_next(si, hash1, ptr);
		simple_index_compare_hash(si, ptr + 1, hash1);
	}

	simple_index_remove_hit(si);

	return 0;
}

static int parse_cmd_line(int argc, char** argv, struct gory_sewer_knob** gsk_in_ptr, struct gory_sewer_knob** gsk_ex_ptr){
	char path[4096];
	struct gory_sewer_knob* gsk_in;
	struct gory_sewer_knob* gsk_ex;
	struct gory_sewer_knob* gsk;
	int i;

	if (argc < 2){
		fprintf(stderr, "[-] usage: file [...] [-e [...]]\n");
		return -1;
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
		return -1;
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

	if (gsk_in->nb_item < 2){
		fprintf(stderr, "[-] in %s, too few include files\n", __func__);

		gory_sewer_delete(gsk_in);
		gory_sewer_delete(gsk_ex);

		return -1;
	}

	*gsk_in_ptr = gsk_in;
	*gsk_ex_ptr = gsk_ex;

	fprintf(stderr, "[+] command line: %lu include file(s), %lu exclude files(s)\n", gsk_in->nb_item, gsk_ex->nb_item);

	return 0;
}

#define START 4
#define SIMPLE 8
#define STOP 512

static int generate(struct gory_sewer_knob* gsk_in, struct last_index* li){
	struct fast_index* fi_buffer[2] = {NULL, NULL};
	int fi_index = 0;
	struct simple_index* si_buffer[2] = {NULL, NULL};
	int si_index = 0;
	struct abs_storage as;
	int status = 0;

	if (abs_storage_init(&as, gsk_in)){
		fprintf(stderr, "[-] in %s, unable to initialize abs storage\n", __func__);
		return -1;
	}

	if (fast_create(gsk_in, &as, START, fi_buffer)){
		fprintf(stderr, "[-] in %s, unable to create index of size %u\n", __func__, START);
		status = -1;
		goto exit;
	}

	fprintf(stderr, "[+] starting fast with %lu pattern(s) of size %zu in inc file(s)\n", fi_buffer[fi_index]->nb_item, fi_buffer[fi_index]->size);

	for (; fi_buffer[fi_index]->nb_item; fi_index = (fi_index + 1) & 0x1){
		if (fi_buffer[fi_index]->size >= SIMPLE){
			break;
		}
		if (fast_next(fi_buffer[fi_index], &as, fi_buffer + ((fi_index + 1) & 0x1))){
			fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, fi_buffer[fi_index]->size + 1);
			status = EXIT_FAILURE;
			goto exit;
		}

		if (!fi_buffer[fi_index]->nb_item){
			continue;
		}

		fast_push(li, fi_buffer[fi_index]);
		fast_index_clean(fi_buffer[fi_index]);
	}

	if (!fi_buffer[fi_index]->nb_item){
		goto exit;
	}

	if (fi_buffer[fi_index]->size > STOP){
		fast_push(li, fi_buffer[fi_index]);
		goto exit;
	}

	fprintf(stderr, "[+] starting simple with %lu pattern(s) of size %zu in inc file(s)\n", fi_buffer[fi_index]->nb_item, fi_buffer[fi_index]->size);

	if (mixed_next(fi_buffer[fi_index], &as, si_buffer)){
		fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, fi_buffer[fi_index]->size + 1);
		status = -1;
		goto exit;
	}

	fast_push(li, fi_buffer[fi_index]);
	fast_index_clean(fi_buffer[fi_index]);

	for ( ; si_buffer[si_index]->nb_item; si_index = (si_index + 1) & 0x1){
		if (si_buffer[si_index]->size > STOP){
			simple_push(li, si_buffer[si_index]);
			break;
		}

		if (simple_next(si_buffer[si_index], &as, si_buffer + ((si_index + 1) & 0x1))){
			fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, si_buffer[si_index]->size + 1);
			status = EXIT_FAILURE;
			break;
		}

		if (!si_buffer[si_index]->nb_item){
			continue;
		}

		simple_push(li, si_buffer[si_index]);
		simple_index_clean(si_buffer[si_index]);
	}

	exit:

	abs_storage_clean(&as);

	if (fi_buffer[0] != NULL){
		fast_index_delete(fi_buffer[0]);
	}
	if (fi_buffer[1] != NULL){
		fast_index_delete(fi_buffer[1]);
	}

	if (si_buffer[0] != NULL){
		simple_index_delete(si_buffer[0]);
	}
	if (si_buffer[1] != NULL){
		simple_index_delete(si_buffer[1]);
	}

	return status;
}

static struct last_index li;

int main(int argc, char** argv){
	struct gory_sewer_knob* gsk_in;
	struct gory_sewer_knob* gsk_ex;
	int status = EXIT_FAILURE;

	if (parse_cmd_line(argc, argv, &gsk_in, &gsk_ex)){
		return EXIT_FAILURE;
	}

	last_index_init(&li, START);

	if (!generate(gsk_in, &li)){
		last_exclude_files(&li, gsk_ex);
		last_index_dump_and_clean(&li, stdout);
		status = EXIT_SUCCESS;
	}

	gory_sewer_delete(gsk_in);
	gory_sewer_delete(gsk_ex);

	return status;
}
