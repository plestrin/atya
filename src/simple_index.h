#ifndef SIMPLE_INDEX_H
#define SIMPLE_INDEX_H

#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "hash.h"

#define STATUS_NONE 0
#define STATUS_HIT 1

struct simple_entry_item {
	uint8_t status;
	const uint8_t* ptr;
};

struct simple_entry {
	uint32_t nb_alloc;
	uint32_t nb_item;
	struct simple_entry_item items[];
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

struct simple_index* simple_index_create(size_t size);

static inline uint16_t simple_index_hash_init(struct simple_index* si, const uint8_t* ptr){
	return hash_init(ptr, si->size);
}

static inline uint16_t simple_index_hash_next(struct simple_index* si, uint16_t hash, const uint8_t* ptr){
	return hash_update(hash, ptr[0], ptr[si->size], si->size);
}

int simple_index_insert_hash(struct simple_index* si, const uint8_t* ptr, uint16_t hash);

static inline int simple_index_insert(struct simple_index* si, const uint8_t* ptr){
	return simple_index_insert_hash(si, ptr, simple_index_hash_init(si, ptr));
}

struct simple_entry_item* simple_index_compare_hash(struct simple_index* si, const uint8_t* ptr, uint16_t hash, uint8_t sel);

uint64_t simple_index_compare_buffer(struct simple_index* si, const uint8_t* buffer, size_t size);

void simple_index_remove(struct simple_index* si, uint8_t sel);

void simple_index_clean(struct simple_index* si);

static inline void simple_index_delete(struct simple_index* si){
	simple_index_clean(si);
	free(si);
}

#endif
