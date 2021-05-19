#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <alloca.h>

#include "fast_index.h"
#include "simple_index.h"
#include "abs_storage.h"
#include "gory_sewer.h"
#include "utile.h"

static void fast_dump(struct fast_index* fi){
	uint8_t* value;
	int cont;
	uint64_t cnt;
	uint64_t size;

	value = alloca(fi->size);

	size = fi->size;
	for (cont = fast_index_get_first(fi, value, 0), cnt = 0; cont; cont = fast_index_get_next(fi, value, 0), cnt ++){
		fwrite(&size, sizeof size, 1, stdout);
		fwrite(value, fi->size, 1, stdout);
	}

	if (cnt){
		fprintf(stderr, "[+] %5lu patterns of size %4zu\n", cnt, fi->size);
	}
}

static void simple_dump(struct simple_index* si){
	const uint8_t* ptr;
	uint64_t iter;
	uint64_t cnt;
	uint64_t size;

	size = si->size;
	for (iter = 0, cnt = 0; simple_index_get(si, &ptr, &iter); iter ++, cnt ++){
		fwrite(&size, sizeof size, 1, stdout);
		fwrite(ptr, si->size, 1, stdout);
	}

	if (cnt){
		fprintf(stderr, "[+] %5lu patterns of size %4zu\n", cnt, si->size);
	}
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

static struct gory_sewer_knob* parse_cmd_line(int argc, char** argv){
	char path[4096];
	struct gory_sewer_knob* file_gsk;
	int i;

	if (argc < 2){
		fprintf(stderr, "[-] usage: file [file ...]\n");
		return NULL;
	}

	if ((file_gsk = gory_sewer_create(0x4000)) == NULL){
		fprintf(stderr, "[-] in %s, unable to create gory sewer\n", __func__);
		return NULL;
	}

	for (i = 1; i < argc; i++){
		strncpy(path, argv[i], sizeof path);
		path[sizeof path - 1] = 0;
		if (list_files(path, sizeof path, file_gsk)){
			fprintf(stderr, "[-] in %s, unable to list files in: %s\n", __func__, path);
		}
	}

	fprintf(stderr, "[+] intersect command line: %lu file(s)\n", file_gsk->nb_item);

	return file_gsk;
}

#define START 6
#define SIMPLE 8
#define STOP 4096

static int generate(struct gory_sewer_knob* file_gsk){
	struct fast_index* fi_buffer[2] = {NULL, NULL};
	int fi_index = 0;
	struct simple_index* si_buffer[2] = {NULL, NULL};
	int si_index = 0;
	struct abs_storage as;
	int status = 0;

	if (abs_storage_init(&as, file_gsk)){
		fprintf(stderr, "[-] in %s, unable to initialize abs storage\n", __func__);
		return -1;
	}

	if (fast_create(file_gsk, &as, START, fi_buffer)){
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

		fast_dump(fi_buffer[fi_index]);
		fast_index_clean(fi_buffer[fi_index]);
	}

	if (!fi_buffer[fi_index]->nb_item){
		goto exit;
	}

	if (fi_buffer[fi_index]->size > STOP){
		fast_dump(fi_buffer[fi_index]);
		goto exit;
	}

	fprintf(stderr, "[+] starting simple with %lu pattern(s) of size %zu in inc file(s)\n", fi_buffer[fi_index]->nb_item, fi_buffer[fi_index]->size);

	if (mixed_next(fi_buffer[fi_index], &as, si_buffer)){
		fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, fi_buffer[fi_index]->size + 1);
		status = -1;
		goto exit;
	}

	fast_dump(fi_buffer[fi_index]);
	fast_index_clean(fi_buffer[fi_index]);

	for ( ; si_buffer[si_index]->nb_item; si_index = (si_index + 1) & 0x1){
		if (si_buffer[si_index]->size > STOP){
			simple_dump(si_buffer[si_index]);
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

		simple_dump(si_buffer[si_index]);
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

int main(int argc, char** argv){
	struct gory_sewer_knob* file_gsk;
	int status = EXIT_FAILURE;

	if ((file_gsk = parse_cmd_line(argc, argv)) == NULL){
		return EXIT_FAILURE;
	}

	if (!generate(file_gsk)){
		status = EXIT_SUCCESS;
	}

	gory_sewer_delete(file_gsk);

	return status;
}
