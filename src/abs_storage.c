#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/stat.h>
#include <errno.h>

#include "abs_storage.h"

#include "utile.h"

#define ABS_STORAGE_MAX_SIZE 0x1000000

static int abs_storage_file_init_skimming(struct abs_storage_file* asf){
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

static int compare_simple_index_file(const char* file_name, struct simple_index* si){
	FILE* handle;
	int status;
	size_t rd_sz;
	size_t ol_sz;

	ol_sz = si->size - 1;

	if ((handle = fopen(file_name, "rb")) == NULL){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, file_name);
		return status;
	}

	rd_sz = fread(chunk, 1, sizeof chunk, handle);
	for ( ; ; ){
		simple_index_compare_buffer(si, chunk, rd_sz);
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

static void fill_simple_index_skim(struct skim* sk, struct simple_index* si){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	uint8_t* data;
	size_t off;
	size_t size;
	size_t i;
	uint16_t hash;

	for (; !skim_iter_get(&ski, &off, &size); ){
		if (size < si->size){
			skim_delete_data(&ski);
			continue;
		}

		data = sk->data + off;

		for (i = 0, hash = simple_index_hash_init(si, data); ; hash = simple_index_hash_next(si, hash, data + i - 1)){
			if (simple_index_insert_hash(si, data + i, hash)){
				fprintf(stderr, "[-] in %s, unable to insert item in simple index. Results may be incorrect\n", __func__);
			}

			if (++i > size - si->size){
				break;
			}
		}

		skim_iter_next(&ski);
	}
}

int abs_storage_fill_simple_index_head(struct abs_storage* as, struct simple_index* si){
	if (!(as->asf_buffer[0].flags & ABS_STORAGE_FILE_FLAG_SK)){
		if (as->asf_buffer[0].size <= ABS_STORAGE_MAX_SIZE && (ABS_STORAGE_MAX_SIZE - as->asf_buffer[0].size) >= as->size){
			if (!abs_storage_file_init_skimming(as->asf_buffer)){
				as->size += as->asf_buffer[0].sk.size;
			}
		}
	}

	if (!(as->asf_buffer[0].flags & ABS_STORAGE_FILE_FLAG_SK)){
		fprintf(stderr, "[-] in %s, fill_file is not implemented\n", __func__);
		return EINVAL;
	}

	as->size -= as->asf_buffer[0].sk.size;
	fill_simple_index_skim(&as->asf_buffer[0].sk, si);
	as->size += as->asf_buffer[0].sk.size;

	return 0;
}

static void compare_simple_index_skim(struct skim* sk, struct simple_index* si){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	uint8_t* data;
	size_t off;
	size_t size;
	size_t i;
	int matched;
	size_t start;
	uint16_t hash;

	for (; !skim_iter_get(&ski, &off, &size); ){
		if (size < si->size){
			skim_delete_data(&ski);
			continue;
		}

		data = sk->data + off;

		for (i = 0, hash = simple_index_hash_init(si, data), matched = 0; ; hash = simple_index_hash_next(si, hash, data + i - 1)){
			if (simple_index_compare_hash(si, data + i, hash, STATUS_HIT)){
				if (!matched){
					start = i;
					matched = 1;
				}
			}
			else if (matched){
				if (i + 1 <= size - si->size){
					if (skim_add_data(sk, off + i + 1, size - (i + 1))){
						fprintf(stderr, "[-] in %s, unable to add data to skim. Results may be incorrect\n", __func__);
					}
				}

				break;
			}

			if (++i > size - si->size){
				break;
			}
		}

		if (!matched){
			skim_delete_data(&ski);
			continue;
		}

		if (skim_resize_data(&ski, off + start, i - start + si->size - 1)){
			fprintf(stderr, "[-] in %s, unable to resize data in skim\n", __func__);
		}

		skim_iter_next(&ski);
	}
}

int abs_storage_intersect_simple_index_tail(struct abs_storage* as, struct simple_index* si){
	uint64_t i;
	int status;

	for (i = 1; i < as->nb_file && si->nb_item; i ++){
		if (!(as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK)){
			if (as->asf_buffer[i].size <= ABS_STORAGE_MAX_SIZE && (ABS_STORAGE_MAX_SIZE - as->asf_buffer[i].size) >= as->size){
				if (!abs_storage_file_init_skimming(as->asf_buffer + i)){
					as->size += as->asf_buffer[i].sk.size;
				}
			}
		}

		if (as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK){
			as->size -= as->asf_buffer[i].sk.size;
			compare_simple_index_skim(&as->asf_buffer[i].sk, si);
			as->size += as->asf_buffer[i].sk.size;
		}
		else if ((status = compare_simple_index_file(as->asf_buffer[i].path, si))){
			fprintf(stderr, "[-] in %s, unable to compare to file: %s\n", __func__, as->asf_buffer[i].path);
			return status;
		}

		simple_index_remove(si, STATUS_NONE);
	}

	return 0;
}

static void abs_stree_skim(struct skim* sk, struct stree* tree, struct simple_index* si, int (*stree_func)(struct stree*, const uint8_t*, size_t, size_t)){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	uint8_t* data;
	size_t off;
	size_t size;
	size_t i;
	int matched;
	size_t start;
	uint16_t hash;

	for (; !skim_iter_get(&ski, &off, &size); ){
		if (size < si->size){
			skim_delete_data(&ski);
			continue;
		}

		data = sk->data + off;

		for (i = 0, hash = simple_index_hash_init(si, data), matched = 0; ; hash = simple_index_hash_next(si, hash, data + i - 1)){
			if (simple_index_compare_hash(si, data + i, hash, STATUS_HIT)){
				if (!matched){
					start = i;
					matched = 1;
				}
			}
			else if (matched){
				if (i + 1 <= size - si->size){
					if (skim_add_data(sk, off + i + 1, size - (i + 1))){
						fprintf(stderr, "[-] in %s, unable to add data to skim. Results may be incorrect\n", __func__);
					}
				}

				break;
			}

			if (++i > size - si->size){
				break;
			}
		}

		if (!matched){
			skim_delete_data(&ski);
			continue;
		}

		if (skim_resize_data(&ski, off + start, i - start + si->size - 1)){
			fprintf(stderr, "[-] in %s, unable to resize data in skim\n", __func__);
		}

		if (stree_func(tree, data + start, i - start + si->size - 1, si->size)){
			fprintf(stderr, "[-] in %s, unable to insert data in suffix tree\n", __func__);
		}

		skim_iter_next(&ski);
	}
}

int abs_storage_fill_stree_head(struct abs_storage* as, struct stree* tree, struct simple_index* si){
	if (!(as->asf_buffer[0].flags & ABS_STORAGE_FILE_FLAG_SK)){
		if (as->asf_buffer[0].size <= ABS_STORAGE_MAX_SIZE && (ABS_STORAGE_MAX_SIZE - as->asf_buffer[0].size) >= as->size){
			if (!abs_storage_file_init_skimming(as->asf_buffer)){
				as->size += as->asf_buffer[0].sk.size;
			}
		}
	}

	if (!(as->asf_buffer[0].flags & ABS_STORAGE_FILE_FLAG_SK)){
		fprintf(stderr, "[-] in %s, fill_file is not implemented\n", __func__);
		return EINVAL;
	}

	as->size -= as->asf_buffer[0].sk.size;
	abs_stree_skim(&as->asf_buffer[0].sk, tree, si, stree_insert_all);
	as->size += as->asf_buffer[0].sk.size;

	return 0;
}

int abs_storage_intersect_stree_tail(struct abs_storage* as, struct stree* tree, struct simple_index* si){
	uint64_t i;

	for (i = 1; i < as->nb_file && si->nb_item; i ++){
		if (!(as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK)){
			if (as->asf_buffer[i].size <= ABS_STORAGE_MAX_SIZE && (ABS_STORAGE_MAX_SIZE - as->asf_buffer[i].size) >= as->size){
				if (!abs_storage_file_init_skimming(as->asf_buffer + i)){
					as->size += as->asf_buffer[i].sk.size;
				}
			}
		}

		if (!(as->asf_buffer[i].flags & ABS_STORAGE_FILE_FLAG_SK)){
			fprintf(stderr, "[-] in %s, compare_file is not implemented\n", __func__);
			return EINVAL;
		}

		as->size -= as->asf_buffer[i].sk.size;
		abs_stree_skim(&as->asf_buffer[i].sk, tree, si, stree_intersect_all);
		as->size += as->asf_buffer[i].sk.size;

		stree_prune(tree);
	}

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
