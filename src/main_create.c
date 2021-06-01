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

	// TODO: remove. This is too expensive.
	for (iter1 = 0; simple_index_get(si_next, &ptr, &iter1); iter1 ++){
		hash1 = simple_index_hash_decrease(si_next, simple_index_iter_get_hash(iter1), ptr);
		simple_index_compare_hash(si, ptr, hash1);
		hash1 = simple_index_hash_next(si, hash1, ptr);
		simple_index_compare_hash(si, ptr + 1, hash1);
	}

	simple_index_remove_hit(si);

	return 0;
}

static struct fast_index* fast_first(struct abs_storage* as, struct gory_sewer_knob* gsk_files, size_t index_size){
	struct fast_index* fi;
	struct abs_index ai;
	char* file_path;

	if ((fi = calloc(sizeof(struct fast_index), 1)) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return NULL;
	}

	fi->size = index_size;

	do {
		if ((file_path = gory_sewer_first(gsk_files)) != NULL){
			if (fast_insert_file(fi, file_path)){
				fprintf(stderr, "[-] in %s, unable to insert file %s to fast index\n", __func__, file_path);
				break;
			}

			abs_index_init_fast(&ai, fi);
			if (abs_storage_intersect(as, &ai, 1)){
				fprintf(stderr, "[-] in %s, unable to intersect files\n", __func__);
				break;
			}
		}

		return fi;
	} while (0);

	free(fi);
	return NULL;
}

static struct simple_index* simple_first(struct abs_storage* as, struct gory_sewer_knob* gsk_files, size_t index_size){
	struct fast_index* fi = NULL;
	struct simple_index* si;
	uint8_t* value;
	int cont;

	if ((fi = fast_first(as, gsk_files, index_size)) == NULL){
		fprintf(stderr, "[-] in %s, unable to create first fast index\n", __func__);
		return NULL;
	}

	if (!simple_index_create(&si, index_size)){
		value = alloca(fi->size);

		for (cont = fast_index_get_first(fi, value, 0); cont; cont = fast_index_get_next(fi, value, 0)){
			if (simple_index_insert(si, value)){
				fprintf(stderr, "[-] in %s, unable to insert\n", __func__);
				simple_index_delete(si);
				si = NULL;
				break;
			}
		}
	}

	fast_index_delete(fi);

	return si;
}

#define START 6
#define STOP 16384

static int create(struct gory_sewer_knob* file_gsk, unsigned int flags){
	struct simple_index* si_buffer[2] = {NULL, NULL};
	int si_index;
	struct abs_storage as;
	int status = 0;

	if ((status = abs_storage_init(&as, file_gsk))){
		fprintf(stderr, "[-] in %s, unable to initialize abs storage\n", __func__);
		return status;
	}

	if ((si_buffer[0] = simple_first(&as, file_gsk, START)) == NULL){
		fprintf(stderr, "[-] in %s, unable to create first simple index\n", __func__);
		status = -1;
		goto exit;
	}

	log_info(flags, "starting with %lu pattern(s) of size %zu in inc file(s)", si_buffer[0]->nb_item, si_buffer[0]->size);

	for (si_index = 0; si_buffer[si_index]->nb_item; si_index = (si_index + 1) & 0x1){
		if (si_buffer[si_index]->size > STOP){
			simple_dump(si_buffer[si_index]);
			break;
		}

		if ((status = simple_next(si_buffer[si_index], &as, si_buffer + ((si_index + 1) & 0x1)))){
			fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, si_buffer[si_index]->size + 1);
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
	unsigned int flags;
	int status = EXIT_SUCCESS;

	if (parse_cmd_line(argc, argv, &file_gsk, &flags)){
		return EXIT_FAILURE;
	}

	if (create(file_gsk, flags)){
		status = EXIT_FAILURE;
	}

	gory_sewer_delete(file_gsk);

	return status;
}
