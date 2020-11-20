#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include "abs_storage.h"

#include "utile.h"

// #define ABS_STORAGE_MAX_SIZE 0x100
#define ABS_STORAGE_MAX_SIZE 0x1000000

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
	as->data_size = 0;

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

static int simple_compare_file(struct simple_index* si, const char* file_name){
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
		simple_index_compare_buffer(si, chunk, rd_sz);
		memmove(chunk, chunk + rd_sz - si->size + 1, si->size - 1);
		rd_sz = fread(chunk + si->size - 1, 1, (sizeof chunk) - si->size + 1, handle);
		if (!rd_sz){
			break;
		}
		rd_sz += si->size - 1;
	}

	fclose(handle);

	return 0;
}

static int simple_compare_file_and_init_skim(struct simple_index* si, const char* file_name, struct skim* sk){
	uint8_t* data = NULL;
	size_t size;
	int status;

	fprintf(stderr, "[+] skimming file: %s\n", file_name);

	if ((status = load_file(file_name, &data, &size))){
		fprintf(stderr, "[-] in %s, unable to load file\n", __func__);
		return status;
	}

	if (skim_init(sk)){
		fprintf(stderr, "[-] in %s, unable to init skim\n", __func__);
		free(data);
		return ENOMEM;
	}

	if (size >= si->size){
		uint64_t match;
		size_t i;
		uint16_t hash;
		size_t moff = 0;
		int moff_set = 0;

		for (i = 0, hash = simple_index_hash_init(si, data); i <= size - si->size; hash = simple_index_hash_update(si, hash, data[i], data[i + si->size - 1]), i++){
			match = simple_index_compare_hash(si, data + i, hash);
			if (match){
				if (!moff_set){
					moff = i;
					moff_set = 1;
				}
			}

			if (moff_set){
				size_t uoff = moff;

				if (!match){
					uoff = i - 1;
				}
				if (i >= size - si->size){
					uoff = i;
				}
				if (moff != uoff){
					if (skim_add_data(sk, data + moff, uoff - moff + si->size)){
						fprintf(stderr, "[-] in %s, unable to add 0x%zx byte(s) to skim structure\n", __func__, uoff - moff + si->size);
					}
					moff_set = 0;
				}
			}
		}
	}

	free(data);

	fprintf(stderr, "[+] result of skimming: 0x%lx / 0x%zx\n", sk->data_size, size);

	return 0;
}

static void simple_compare_skim(struct simple_index* si, struct skim* sk){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	uint8_t* data;
	size_t size;
	size_t i;
	uint16_t hash;


	for (; !skim_iter_get(&ski, &data, &size); ){
		if (size < si->size){
			skim_delete_data(sk, &ski);
			continue;
		}

		/* TODO : improve */
		for (i = 0, hash = simple_index_hash_init(si, data); i <= size - si->size; hash = simple_index_hash_update(si, hash, data[i], data[i + si->size - 1]), i++){
			simple_index_compare_hash(si, data + i, hash);
		}

		skim_iter_next(&ski);
	}
}

int abs_storage_simple_intersect(struct abs_storage* as, struct simple_index* si){
	uint64_t i;
	int status = 0;

	for (i = 0; i < as->nb_file; i ++){
		if (as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK){
			simple_compare_skim(si, &as->asf_buffer[i].sk);
		}
		else {
			if (as->asf_buffer[i].size <= ABS_STORAGE_MAX_SIZE && (ABS_STORAGE_MAX_SIZE - as->asf_buffer[i].size) >= as->data_size && !simple_compare_file_and_init_skim(si, as->asf_buffer[i].path, &as->asf_buffer[i].sk)){
				as->asf_buffer[i].flags |= ABS_STORAGE_FILE_FLAG_SK;
				as->data_size += as->asf_buffer[i].sk.data_size;
			}
			else {
				if ((status = simple_compare_file(si, as->asf_buffer[i].path))){
					fprintf(stderr, "[-] in %s, unable to compare to file: %s\n", __func__, as->asf_buffer[i].path);
					break;
				}
			}
		}

		simple_index_remove_nohit(si);
		if (!si->nb_item){
			break;
		}
	}

	return status;
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
