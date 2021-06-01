#ifndef SIMPLE_INDEX_H
#define SIMPLE_INDEX_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hash.h"

#define STATUS_NONE 0
#define STATUS_HIT 1
#define STATUS_PRO 2

struct __attribute__((__packed__)) simple_entry_item {
	struct simple_entry_item* lo;
	struct simple_entry_item* hi;
	uint8_t status;
	uint8_t data[];
};

struct simple_entry {
	uint32_t nb_alloc;
	uint32_t nb_item;
};

struct simple_index {
	size_t size;
	uint64_t nb_item;
	struct simple_entry* index[0x10000];
};

static inline void simple_index_init(struct simple_index* si, size_t size){
	si->size = size;
	si->nb_item = 0;
	memset(&si->index, 0, sizeof si->index);
}

int simple_index_create(struct simple_index** si, size_t size);

static inline uint16_t simple_index_hash_init(struct simple_index* si, const uint8_t* value){
	return hash_init(value, si->size - 1);
}

static inline uint16_t simple_index_hash_next(struct simple_index* si, uint16_t hash, const uint8_t* value){
	return hash_update(hash, value[0], value[si->size - 1], si->size - 1);
}

static inline uint16_t simple_index_hash_decrease(struct simple_index* si, uint16_t hash, const uint8_t* value){
	return hash_pop(hash, value[si->size - 2]);
}

static inline uint16_t simple_index_hash_increase(struct simple_index* si, uint16_t hash, const uint8_t* value){
	return hash_push(hash, value[si->size - 1]);
}

int simple_index_insert_hash(struct simple_index* si, const uint8_t* value, uint16_t hash, struct simple_entry_item* lo, struct simple_entry_item* hi);

static inline int simple_index_insert(struct simple_index* si, const uint8_t* value){
	return simple_index_insert_hash(si, value, simple_index_hash_init(si, value), NULL, NULL);
}

uint64_t simple_index_compare_hash(struct simple_index* si, const uint8_t* value, uint16_t hash);

uint64_t simple_index_compare_buffer(struct simple_index* si, const uint8_t* buffer, size_t size);

static inline uint64_t simple_index_iter_init(uint16_t hash, uint32_t item_idx) {
	return (uint64_t)item_idx | ((uint64_t)hash << 32);
}

static inline uint16_t simple_index_iter_get_hash(uint64_t iter){
	return iter >> 32;
}

static inline uint32_t simple_index_iter_get_item_idx(uint64_t iter){
	return  iter;
}

struct simple_entry_item* simple_index_iter_get_item(struct simple_index* si, uint64_t iter);

int simple_index_get(struct simple_index* si, const uint8_t** value_ptr, uint64_t* iter);

int simple_index_get_cpy(struct simple_index* si, uint8_t* value, uint64_t* iter);

int simple_index_get_masked_cpy(struct simple_index* si, uint8_t* value, uint64_t* iter);

uint64_t simple_index_remove(struct simple_index* si, uint8_t sel);

void simple_index_clean(struct simple_index* si);

static inline void simple_index_delete(struct simple_index* si){
	simple_index_clean(si);
	free(si);
}

#endif
