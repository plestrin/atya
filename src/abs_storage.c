#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include "abs_storage.h"

#include "utile.h"

#define ABS_STORAGE_MAX_SIZE 0x1000000

static size_t abs_storage_file_get_mem_size(struct abs_storage_file* asf){
	if (asf->flags & ABS_STORAGE_FILE_FLAG_SK){
		return asf->sk.size;
	}

	return 0;
}

static int abs_storage_init_skimming(struct abs_storage_file* asf){
	uint8_t* data;
	size_t size;
	int status;

	if (asf->flags & ABS_STORAGE_FILE_FLAG_SK){
		fprintf(stderr, "[-] in %s, skimming has already been initialized for this file\n", __func__);
		return 0;
	}

	if ((status = load_file(asf->path, &data, &size))){
		fprintf(stderr, "[-] in %s, unable to load file\n", __func__);
		return status;
	}

	if ((status = skim_init(&asf->sk, data, size))){
		fprintf(stderr, "[-] in %s, unable to initialized skim\n", __func__);
		free(data);
		return status;
	}

	asf->flags |= ABS_STORAGE_FILE_FLAG_SK;

	return 0;
}

int abs_storage_init(struct abs_storage* as, struct gory_sewer_knob* gsk_path){
	char* file_path;
	uint64_t i;
	struct stat statbuf;

	as->gsk_path = gsk_path;

	if (!gsk_path->nb_item){
		fprintf(stderr, "[-] in %s, GSK is empty\n", __func__);
		return EINVAL;
	}

	as->nb_file = gsk_path->nb_item;
	as->size = 0;

	if ((as->asf_buffer = malloc(sizeof(struct abs_storage_file) * gsk_path->nb_item)) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}

	for (file_path = gory_sewer_first(gsk_path), i = 0; file_path != NULL; file_path = gory_sewer_next(gsk_path), i ++){
		if (i >= as->nb_file){
			fprintf(stderr, "[-] in %s, GSK iterator returns to many items, discard\n", __func__);
			break;
		}

		as->asf_buffer[i].path = file_path;
		as->asf_buffer[i].flags = 0;

		if (stat(file_path, &statbuf)){
			fprintf(stderr, "[-] in %s, unable to get file \"%s\" size\n", __func__, file_path);
			as->asf_buffer[i].size = 0xffffffffffffffff;
		}
		else {
			as->asf_buffer[i].size = statbuf.st_size;
		}
	}

	return 0;
}

static uint8_t chunk[0x10000];

static int compare_file(struct abs_index* ai, const char* file_name){
	FILE* handle;
	int status;
	size_t rd_sz;
	size_t ol_sz;

	ol_sz = abs_index_get_size(ai) - 1;

	if ((handle = fopen(file_name, "rb")) == NULL){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, file_name);
		return status;
	}

	rd_sz = fread(chunk, 1, sizeof chunk, handle);
	for ( ; ; ){
		abs_index_compare_buffer(ai, chunk, rd_sz);
		memmove(chunk, chunk + rd_sz - ol_sz, ol_sz);
		rd_sz = fread(chunk + ol_sz, 1, (sizeof chunk) - ol_sz, handle);
		if (!rd_sz){
			break;
		}
		rd_sz += ol_sz;
	}

	fclose(handle);

	return 0;
}

static void compare_skim(struct abs_index* ai, struct skim* sk){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	uint8_t* data;
	size_t off;
	size_t size;
	size_t i;
	int matched;
	size_t start;
	size_t id_sz;
	uint64_t cmp_res;

	id_sz = abs_index_get_size(ai);

	for (; !skim_iter_get(&ski, &off, &size); ){
		if (size < id_sz){
			skim_delete_data(&ski);
			continue;
		}

		data = sk->data + off;

		for (i = 0, cmp_res = abs_index_compare_init(ai, data), matched = 0; ; cmp_res = abs_index_compare_next(ai, data + i - 1)){
			if (cmp_res){
				if (!matched){
					start = i;
					matched = 1;
				}
			}
			else if (matched){
				if (start + 1 == i){
					matched = 0;
				}
				else {
					if (i + 1 <= size - id_sz){
						if (skim_add_data(sk, off + i + 1, size - (i + 1))){
							fprintf(stderr, "[-] in %s, unable to add data to skim. Results may be incorrect\n", __func__);
						}
					}

					break;
				}
			}

			if (++i > size - id_sz){
				break;
			}
		}

		if (!matched){
			skim_delete_data(&ski);
			continue;
		}

		if (skim_resize_data(&ski, off + start, i - start + id_sz - 1)){
			fprintf(stderr, "[-] in %s, unable to resize data in skim\n", __func__);
		}

		skim_iter_next(&ski);
	}
}

int abs_storage_intersect(struct abs_storage* as, struct abs_index* ai, uint64_t offset){
	uint64_t i;
	uint64_t old_size;
	uint64_t new_size;
	int status;

	old_size = as->size;
	new_size = 0;

	for (i = offset; i < as->nb_file; i ++){
		if (!(as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK)){
			if (as->asf_buffer[i].size <= ABS_STORAGE_MAX_SIZE && (ABS_STORAGE_MAX_SIZE - as->asf_buffer[i].size) >= old_size){
				if (!abs_storage_init_skimming(as->asf_buffer + i)){
					old_size += as->asf_buffer[i].sk.size;
				}
			}
		}

		if (as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK){
			compare_skim(ai, &as->asf_buffer[i].sk);
		}
		else if ((status = compare_file(ai, as->asf_buffer[i].path))){
			fprintf(stderr, "[-] in %s, unable to compare to file: %s\n", __func__, as->asf_buffer[i].path);
			return status;
		}

		new_size += abs_storage_file_get_mem_size(as->asf_buffer + i);

		if (i + 1 < as->nb_file){
			if (!abs_index_remove_nohit(ai)){
				as->size = old_size;
				return 0;
			}
		}
		else if (!abs_index_remove_nohitpro(ai)){
			as->size = old_size;
			return 0;
		}
	}

	as->size = new_size;

	return 0;
}

void abs_storage_clean(struct abs_storage* as){
	uint64_t i;

	for (i = 0; i < as->nb_file; i ++){
		if (as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK){
			skim_clean(&as->asf_buffer[i].sk);
		}
	}
	free(as->asf_buffer);
}
