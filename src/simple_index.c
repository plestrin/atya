#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "simple_index.h"

static inline size_t simple_entry_get_item_size(size_t size){
	return sizeof(struct simple_entry_item) + size;
}

static inline struct simple_entry_item* simple_entry_get_item(struct simple_entry* se, uint32_t idx, size_t size){
	return (struct simple_entry_item*)((uint8_t*)(se + 1) + idx * simple_entry_get_item_size(size));
}

#define SIMPLE_INDEX_ALLOC_STEP 8

static int simple_entry_insert(struct simple_entry** se, const uint8_t* value, size_t size, struct simple_entry_item* lo, struct simple_entry_item* hi){
	struct simple_entry* new_se;
	struct simple_entry_item* item;

	if (*se == NULL){
		if ((new_se = malloc(sizeof(struct simple_entry) + simple_entry_get_item_size(size) * SIMPLE_INDEX_ALLOC_STEP)) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}

		*se = new_se;
		new_se->nb_alloc = SIMPLE_INDEX_ALLOC_STEP;
		new_se->nb_item = 0;
	}
	else if ((*se)->nb_item == (*se)->nb_alloc){
		if ((new_se = realloc(*se, sizeof(struct simple_entry) + simple_entry_get_item_size(size) * (SIMPLE_INDEX_ALLOC_STEP + (*se)->nb_alloc))) == NULL){
			fprintf(stderr, "[-] in %s, unable to reallocate memory\n", __func__);
			return ENOMEM;
		}

		*se = new_se;
		new_se->nb_alloc += SIMPLE_INDEX_ALLOC_STEP;
	}

	item = simple_entry_get_item(*se, (*se)->nb_item, size);
	item->lo = lo;
	item->hi = hi;
	item->status = STATUS_NONE;
	memcpy(item->data, value, size);
	(*se)->nb_item ++;

	return 0;
}

static uint64_t simple_entry_compare(struct simple_entry* se, const uint8_t* value, size_t size){
	uint32_t i;
	struct simple_entry_item* item;

	for (i = 0; i < se->nb_item; i++){
		item = simple_entry_get_item(se, i, size);
		if (!memcmp(item->data, value, size)){
			item->status |= STATUS_HIT;
			return 1;
		}
	}

	return 0;
}

static uint32_t simple_entry_remove(struct simple_entry* se, size_t size, uint8_t sel){
	uint32_t i;
	uint32_t j;
	struct simple_entry_item* item_i;
	struct simple_entry_item* item_j;

	for (i = 0, j = se->nb_item; i < se->nb_item; i++){
		item_i = simple_entry_get_item(se, i, size);
		if ((item_i->status & STATUS_HIT) == (sel & STATUS_HIT)){
			for (j--; j > i; j--){
				item_j = simple_entry_get_item(se, j, size);
				if ((item_j->status & STATUS_HIT) != (sel & STATUS_HIT)){
					memcpy(item_i, item_j, simple_entry_get_item_size(size));
					break;
				}
			}

			se->nb_item = j;
		}

		if (sel & STATUS_PRO){
			if (item_i->lo != NULL){
				item_i->lo->status |= item_i->status & STATUS_HIT;
			}
			if (item_i->hi != NULL){
				item_i->hi->status |= item_i->status & STATUS_HIT;
			}
		}
		item_i->status = STATUS_NONE;
	}

	return j;
}

static uint32_t simple_entry_dump(struct simple_entry* se, size_t size, uint8_t sel, FILE* stream){
	uint32_t i;
	uint32_t cnt;
	uint64_t size_;
	struct simple_entry_item* item;

	size_ = size;
	for (i = 0, cnt = 0; i < se->nb_item; i++){
		item = simple_entry_get_item(se, i, size);
		if ((item->status & STATUS_HIT) == (sel & STATUS_HIT)){
			fwrite(&size_, sizeof size_, 1, stream);
			fwrite(item->data, size, 1, stream);
		}
	}

	return cnt;
}

static const uint8_t* simple_entry_get_item_data(struct simple_entry* se, uint32_t idx, size_t size){
	if (idx < se->nb_item){
		return simple_entry_get_item(se, idx, size)->data;
	}

	return NULL;
}

static int simple_entry_get_masked(struct simple_entry* se, uint8_t* value, uint32_t* idx, size_t size){
	uint32_t i;
	struct simple_entry_item* item;

	for (i = *idx; i < se->nb_item; i++){
		item = simple_entry_get_item(se, i, size);
		if (!memcmp(item->data, value, size - 1)){
			value[size - 1] = item->data[size - 1];
			*idx = i;
			return 1;
		}
	}

	return 0;
}

int simple_index_create(struct simple_index** si, size_t size){
	if ((*si = malloc(sizeof(struct simple_index))) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}

	simple_index_init(*si, size);

	return 0;
}

int simple_index_insert_hash(struct simple_index* si, const uint8_t* value, uint16_t hash, struct simple_entry_item* lo, struct simple_entry_item* hi){
	int status;

	if (!(status = simple_entry_insert(si->index + hash, value, si->size, lo, hi))){
		si->nb_item ++;
	}
	return status;
}

uint64_t simple_index_compare_hash(struct simple_index* si, const uint8_t* value, uint16_t hash){
	if (si->index[hash] != NULL){
		return simple_entry_compare(si->index[hash], value, si->size);
	}

	return 0;
}

uint64_t simple_index_compare_buffer(struct simple_index* si, const uint8_t* buffer, size_t size){
	size_t i;
	uint16_t hash;
	uint64_t cnt;

	if (size < si->size){
		return 0;
	}

	hash = simple_index_hash_init(si, buffer);
	cnt = simple_index_compare_hash(si, buffer, hash);

	for (i = 1; i < size - si->size + 1; i++){
		hash = simple_index_hash_next(si, hash, buffer + i - 1);
		cnt += simple_index_compare_hash(si, buffer + i, hash);
	}

	return cnt;
}

int simple_index_get(struct simple_index* si, const uint8_t** value_ptr, uint64_t* iter){
	uint32_t i;
	uint32_t j;

	for (i = simple_index_iter_get_hash(*iter), j = simple_index_iter_get_item_idx(*iter); i < 0x10000; i ++, j = 0){
		if (si->index[i] == NULL){
			continue;
		}

		if ((*value_ptr = simple_entry_get_item_data(si->index[i], j, si->size)) != NULL){
			*iter = simple_index_iter_init(i, j);
			return 1;
		}
	}

	return 0;
}

int simple_index_get_cpy(struct simple_index* si, uint8_t* value, uint64_t* iter){
	const uint8_t* ptr;

	if (simple_index_get(si, &ptr, iter)){
		memcpy(value, ptr, si->size);
		return 1;
	}

	return 0;
}

struct simple_entry_item* simple_index_iter_get_item(struct simple_index* si, uint64_t iter){
	return simple_entry_get_item(si->index[simple_index_iter_get_hash(iter)], simple_index_iter_get_item_idx(iter), si->size);
}

int simple_index_get_masked_cpy(struct simple_index* si, uint8_t* value, uint64_t* iter){
	uint16_t hash;
	uint32_t item_idx;
	int status;

	hash = simple_index_iter_get_hash(*iter);
	if (si->index[hash] == NULL){
		return 0;
	}

	item_idx = simple_index_iter_get_item_idx(*iter);
	status = simple_entry_get_masked(si->index[hash], value, &item_idx, si->size);
	*iter = simple_index_iter_init(hash, item_idx);

	return status;
}

uint64_t simple_index_remove(struct simple_index* si, uint8_t sel){
	uint32_t i;
	uint32_t local_cnt;
	uint64_t cnt;

	for (i = 0, cnt = 0; i < 0x10000; i++){
		if (si->index[i] != NULL){
			local_cnt = simple_entry_remove(si->index[i], si->size, sel);
			if (!local_cnt){
				free(si->index[i]);
				si->index[i] = NULL;
			}
			else {
				cnt += local_cnt;
			}
		}
	}
	si->nb_item = cnt;

	return cnt;
}

uint64_t simple_index_dump_and_clean(struct simple_index* si, uint8_t sel, FILE* stream){
	uint32_t i;
	uint64_t cnt;

	for (i = 0, cnt = 0; i < 0x10000; i++){
		if (si->index[i] != NULL){
			cnt += simple_entry_dump(si->index[i], si->size, sel, stream);
			free(si->index[i]);
			si->index[i] = NULL;
		}
	}
	si->nb_item = 0;

	return cnt;
}

void simple_index_clean(struct simple_index* si){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		free(si->index[i]);
		si->index[i] = NULL;
	}
	si->nb_item = 0;
}
