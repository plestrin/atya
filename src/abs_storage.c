#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "abs_storage.h"

#include "utile.h"

static int abs_storage_file_init(struct abs_storage_file* asf, const char* path){
	uint8_t* data;
	size_t size;
	int status;

	if ((status = load_file(path, &data, &size))){
		fprintf(stderr, "[-] in %s, unable to load file\n", __func__);
		return status;
	}

	if ((status = skim_init(&asf->sk, size))){
		fprintf(stderr, "[-] in %s, unable to initialized skim\n", __func__);
		free(data);
		return status;
	}

	asf->data = data;

	return 0;
}

int abs_storage_init(struct abs_storage* as, struct gory_sewer_knob* gsk_path){
	char* file_path;
	uint64_t i;
	int status = 0;

	if (!gsk_path->nb_item){
		fprintf(stderr, "[-] in %s, GSK is empty\n", __func__);
		return EINVAL;
	}

	as->nb_file = gsk_path->nb_item;

	if ((as->asf_buffer = malloc(sizeof(struct abs_storage_file) * as->nb_file)) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}

	for (file_path = gory_sewer_first(gsk_path), i = 0; file_path != NULL; file_path = gory_sewer_next(gsk_path), i++){
		if (i >= as->nb_file){
			fprintf(stderr, "[-] in %s, GSK iterator returns to many items, discard\n", __func__);
			break;
		}

		if ((status = abs_storage_file_init(as->asf_buffer + i, file_path))){
			fprintf(stderr, "[-] in %s, unable to initialize abs storage file\n", __func__);
			break;
		}
	}

	as->nb_file = i;

	return status;
}

static int fill_simple_index_skim(struct skim* sk, struct simple_index* si, const uint8_t* data){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	const uint8_t* ptr;
	size_t off;
	size_t size;
	size_t i;
	uint16_t hash;
	int status = 0;

	for (; !skim_iter_get(&ski, &off, &size); ){
		if (size < si->size){
			skim_delete_data(&ski);
			continue;
		}

		ptr = data + off;

		for (i = 0, hash = simple_index_hash_init(si, ptr); ; hash = simple_index_hash_next(si, hash, ptr + i - 1)){
			if (simple_index_insert_hash(si, ptr + i, hash)){
				fprintf(stderr, "[-] in %s, unable to insert item in simple index. Results may be incorrect\n", __func__);
				status = ENOMEM;
			}

			if (++i > size - si->size){
				break;
			}
		}

		skim_iter_next(&ski);
	}

	return status;
}

int abs_storage_fill_simple_index_head(struct abs_storage* as, struct simple_index* si){
	return fill_simple_index_skim(&as->asf_buffer[0].sk, si, as->asf_buffer[0].data);
}

static int compare_simple_index_skim(struct skim* sk, struct simple_index* si, const uint8_t* data){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	const uint8_t* ptr;
	size_t off;
	size_t size;
	size_t i;
	int matched;
	size_t start;
	uint16_t hash;
	int status = 0;

	for (; !skim_iter_get(&ski, &off, &size); ){
		if (size < si->size){
			skim_delete_data(&ski);
			continue;
		}

		ptr = data + off;

		for (i = 0, hash = simple_index_hash_init(si, ptr), matched = 0; ; hash = simple_index_hash_next(si, hash, ptr + i - 1)){
			if (simple_index_compare_hash(si, ptr + i, hash, STATUS_HIT)){
				if (!matched){
					start = i;
					matched = 1;
				}
			}
			else if (matched){
				if (i + 1 <= size - si->size){
					if (skim_add_data(sk, off + i + 1, size - (i + 1))){
						fprintf(stderr, "[-] in %s, unable to add data to skim. Results may be incorrect\n", __func__);
						status = ENOMEM;
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
			status = ENOMEM;
		}

		skim_iter_next(&ski);
	}

	return status;
}

int abs_storage_intersect_simple_index_tail(struct abs_storage* as, struct simple_index* si){
	uint64_t i;
	int status;

	for (i = 1; i < as->nb_file; i++){
		if ((status = compare_simple_index_skim(&as->asf_buffer[i].sk, si, as->asf_buffer[i].data))){
			fprintf(stderr, "[-] in %s, unable to compare to simple index\n", __func__);
			return status;
		}

		simple_index_remove(si, STATUS_NONE);
	}

	return 0;
}

static int abs_stree_skim(struct skim* sk, struct stree* tree, struct simple_index* si, const uint8_t* data, int (*stree_func)(struct stree*, const uint8_t*, size_t, size_t)){
	struct skim_iter ski = SKIM_ITER_INIT(sk);
	const uint8_t* ptr;
	size_t off;
	size_t size;
	size_t i;
	int matched;
	size_t start;
	uint16_t hash;
	int status = 0;

	for (; !skim_iter_get(&ski, &off, &size); ){
		if (size < si->size){
			skim_delete_data(&ski);
			continue;
		}

		ptr = data + off;

		for (i = 0, hash = simple_index_hash_init(si, ptr), matched = 0; ; hash = simple_index_hash_next(si, hash, ptr + i - 1)){
			if (simple_index_compare_hash(si, ptr + i, hash, STATUS_HIT)){
				if (!matched){
					start = i;
					matched = 1;
				}
			}
			else if (matched){
				if (i + 1 <= size - si->size){
					if (skim_add_data(sk, off + i + 1, size - (i + 1))){
						fprintf(stderr, "[-] in %s, unable to add data to skim. Results may be incorrect\n", __func__);
						status = ENOMEM;
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
			status = ENOMEM;
		}

		if (stree_func(tree, ptr + start, i - start + si->size - 1, si->size)){
			fprintf(stderr, "[-] in %s, unable to process data in suffix tree\n", __func__);
			status = ENOMEM;
		}

		skim_iter_next(&ski);
	}

	return status;
}

int abs_storage_fill_stree_head(struct abs_storage* as, struct stree* tree, struct simple_index* si){
	return abs_stree_skim(&as->asf_buffer[0].sk, tree, si, as->asf_buffer[0].data, stree_insert_all);
}

int abs_storage_intersect_stree_tail(struct abs_storage* as, struct stree* tree, struct simple_index* si){
	uint64_t i;
	int status;

	for (i = 1; i < as->nb_file; i ++){
		if ((status = abs_stree_skim(&as->asf_buffer[i].sk, tree, si, as->asf_buffer[i].data, stree_intersect_all))){
			fprintf(stderr, "[-] in %s, unable to compare to suffix tree\n", __func__);
			return status;
		}

		stree_prune(tree);
	}

	return 0;
}

void abs_storage_clean(struct abs_storage* as){
	uint64_t i;

	for (i = 0; i < as->nb_file; i ++){
		skim_clean(&as->asf_buffer[i].sk);
		free(as->asf_buffer[i].data);
	}

	free(as->asf_buffer);
}
