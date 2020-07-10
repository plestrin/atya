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
	struct simple_entry* index[0x10000];
};

static inline void simple_index_init(struct simple_index* si, size_t size){
	si->size = size;
	memset(&si->index, 0, sizeof si->index);
}

int simple_index_create(struct simple_index** si, size_t size);

static inline uint16_t simple_index_hash_init(struct simple_index* si, const uint8_t* value){
	return hash_init(value, si->size - 1);
}

static inline uint16_t simple_index_hash_update(struct simple_index* si, uint16_t hash, uint8_t old, uint8_t new){
	return hash_update(hash, old, new, si->size - 1);
}

int simple_index_insert_hash(struct simple_index* si, const uint8_t* value, uint16_t hash);

static inline int simple_index_insert(struct simple_index* si, const uint8_t* value){
	return simple_index_insert_hash(si, value, simple_index_hash_init(si, value));
}

uint64_t simple_index_count(struct simple_index* si);

uint64_t simple_index_compare_hash(struct simple_index* si, const uint8_t* value, uint16_t hash);

static inline uint64_t simple_index_compare(struct simple_index* si, const uint8_t* value){
	return simple_index_compare_hash(si, value, simple_index_hash_init(si, value));
}

uint64_t simple_index_compare_buffer(struct simple_index* si, const uint8_t* buffer, size_t size);

int simple_index_get(struct simple_index* si, uint8_t* value, uint64_t* iter);

static inline int simple_index_get_next(struct simple_index* si, uint8_t* value, uint64_t* iter){
	*iter += 1;
	return simple_index_get(si, value, iter);
}

int simple_index_get_hash(struct simple_index* si, uint8_t* value, uint16_t hash, uint32_t* iter);

static inline int simple_index_get_hash_next(struct simple_index* si, uint8_t* value, uint16_t hash, uint32_t* iter){
	*iter += 1;
	return simple_index_get_hash(si, value, hash, iter);
}

uint64_t simple_index_remove_hit(struct simple_index* si);
uint64_t simple_index_count_hit(struct simple_index* si);
uint64_t simple_index_remove_nohit(struct simple_index* si);
uint64_t simple_index_count_nohit(struct simple_index* si);

void simple_index_dump(struct simple_index* si);

void simple_index_clean(struct simple_index* si);

static inline void simple_index_delete(struct simple_index* si){
	simple_index_clean(si);
	free(si);
}

#endif
