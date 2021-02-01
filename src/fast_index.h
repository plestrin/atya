#ifndef FAST_INDEX_H
#define FAST_INDEX_H

#include <stdlib.h>
#include <stdint.h>

struct fast_index {
	size_t size;
	uint64_t nb_item;
	void* root[0x10000];
};

int fast_index_insert(struct fast_index* fi, const uint8_t* value);
int fast_index_insert_buffer(struct fast_index* fi, const uint8_t* buffer, size_t size);

int fast_index_get_first(struct fast_index* fi, uint8_t* value, size_t mask_size);
int fast_index_get_next(struct fast_index* fi, uint8_t* value, size_t mask_size);

uint64_t fast_index_compare(struct fast_index* fi, const uint8_t* value);
uint64_t fast_index_compare_buffer(struct fast_index* fi, const uint8_t* buffer, size_t size);

uint64_t fast_index_remove(struct fast_index* fi, uint64_t sel);

static inline uint64_t fast_index_remove_hit(struct fast_index* fi){
	return fast_index_remove(fi, 0x5555555555555555);
}

static inline uint64_t fast_index_remove_nohit(struct fast_index* fi){
	return fast_index_remove(fi, 0x0000000000000000);
}

void fast_index_clean(struct fast_index* fi);
void fast_index_delete(struct fast_index* fi);

#endif
