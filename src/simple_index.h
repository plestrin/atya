#ifndef SIMPLE_INDEX_H
#define SIMPLE_INDEX_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hash.h"

struct simple_entry {
	uint32_t nb_alloc;
	uint32_t nb_used;
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

int simple_index_insert_hash(struct simple_index* si, const uint8_t* value, uint16_t hash);

static inline int simple_index_insert(struct simple_index* si, const uint8_t* value){
	return simple_index_insert_hash(si, value, simple_index_hash_init(si, value));
}

uint64_t simple_index_compare_hash(struct simple_index* si, const uint8_t* value, uint16_t hash);

uint64_t simple_index_compare_buffer(struct simple_index* si, const uint8_t* buffer, size_t size);

int simple_index_get(struct simple_index* si, const uint8_t** value_ptr, uint64_t* iter);

int simple_index_get_cpy(struct simple_index* si, uint8_t* value, uint64_t* iter);

#define simple_index_iter_get_hash(iter) ((iter >> 32) & 0xffff)

int simple_index_get_hash(struct simple_index* si, uint8_t* value, uint16_t hash, uint32_t* iter);

uint64_t simple_index_remove(struct simple_index* si, uint8_t sel);

static inline uint64_t simple_index_remove_hit(struct simple_index* si){
	return simple_index_remove(si, 1);
}

static inline uint64_t simple_index_remove_nohit(struct simple_index* si){
	return simple_index_remove(si, 0);
}

void simple_index_clean(struct simple_index* si);

static inline void simple_index_delete(struct simple_index* si){
	simple_index_clean(si);
	free(si);
}

#endif
