#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "simple_index.h"

static inline uint8_t* simple_entry_get_item(struct simple_entry* se, uint32_t idx, size_t size){
	return (uint8_t*)(se + 1) + idx * (size + 1);
}

#define SIMPLE_INDEX_ALLOC_STEP 8

static int simple_entry_insert(struct simple_entry** se, const uint8_t* value, size_t size){
	struct simple_entry* new_se;
	uint8_t* item;

	if (*se == NULL){
		if ((new_se = malloc(sizeof(struct simple_entry) + (size + 1) * SIMPLE_INDEX_ALLOC_STEP)) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}

		*se = new_se;
		new_se->nb_alloc = SIMPLE_INDEX_ALLOC_STEP;
		new_se->nb_used = 0;
	}
	else if ((*se)->nb_used == (*se)->nb_alloc){
		if ((new_se = realloc(*se, sizeof(struct simple_entry) + (size + 1) * (SIMPLE_INDEX_ALLOC_STEP + (*se)->nb_alloc))) == NULL){
			fprintf(stderr, "[-] in %s, unable to reallocate memory\n", __func__);
			return ENOMEM;
		}

		*se = new_se;
		new_se->nb_alloc += SIMPLE_INDEX_ALLOC_STEP;
	}

	item = simple_entry_get_item(*se, (*se)->nb_used, size);
	item[0] = 0;
	memcpy(item + 1, value, size);
	(*se)->nb_used ++;

	return 0;
}

static uint64_t simple_entry_compare(struct simple_entry* se, const uint8_t* value, size_t size){
	uint32_t i;
	uint8_t* item;

	for (i = 0; i < se->nb_used; i++){
		item = simple_entry_get_item(se, i, size);
		if (!memcmp(item + 1, value, size)){
			item[0] = 1;
			return 1;
		}
	}

	return 0;
}

static uint32_t simple_entry_remove(struct simple_entry* se, size_t size, uint8_t sel){
	uint32_t i;
	uint32_t j;
	uint8_t* item_src;
	uint8_t* item_dst;

	for (i = 0, j = 0; i < se->nb_used; i++){
		item_src = simple_entry_get_item(se, i, size);
		if (item_src[0] != sel){
			item_dst = simple_entry_get_item(se, j, size);
			item_dst[0] = 0;
			if (i != j){
				memcpy(item_dst + 1, item_src + 1, size);
			}
			j ++;
		}
	}

	se->nb_used = j;

	return j;
}

static int simple_entry_get(struct simple_entry* se, uint8_t* value, uint32_t* idx, size_t size){
	if (*idx < se->nb_used){
		memcpy(value, simple_entry_get_item(se, *idx, size) + 1, size);
		return 1;
	}

	return 0;
}

static int simple_entry_get_masked(struct simple_entry* se, uint8_t* value, uint32_t* idx, size_t size){
	uint32_t i;
	uint8_t* item;

	for (i = *idx; i < se->nb_used; i++){
		item = simple_entry_get_item(se, i, size);
		if (!memcmp(item + 1, value, size - 1)){
			value[size - 1] = item[size];
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

int simple_index_insert_hash(struct simple_index* si, const uint8_t* value, uint16_t hash){
	int status;

	if (!(status = simple_entry_insert(si->index + hash, value, si->size))){
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

int simple_index_get(struct simple_index* si, uint8_t* value, uint64_t* iter){
	uint32_t i;
	uint32_t j;

	for (i = *iter >> 32, j = *iter; i < 0x10000; i ++, j = 0){
		if (si->index[i] == NULL){
			continue;
		}
		if (simple_entry_get(si->index[i], value, &j, si->size)){
			*iter = j | ((uint64_t)i << 32);
			return 1;
		}
	}

	return 0;
}

int simple_index_get_hash(struct simple_index* si, uint8_t* value, uint16_t hash, uint32_t* iter){
	if (si->index[hash] != NULL){
		return simple_entry_get_masked(si->index[hash], value, iter, si->size);
	}

	return 0;
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

void simple_index_clean(struct simple_index* si){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		free(si->index[i]);
		si->index[i] = NULL;
	}
	si->nb_item = 0;
}
