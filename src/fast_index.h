#ifndef FAST_INDEX_H
#define FAST_INDEX_H

#include <stdlib.h>
#include <stdint.h>

struct fast_index {
	size_t size;
	void* root[0x10000];
};

int fast_index_insert(struct fast_index* ir, const uint8_t* value);
int fast_index_insert_buffer(struct fast_index* ir, const uint8_t* buffer, size_t size);

uint64_t fast_index_count(struct fast_index* ir);

int fast_index_get_first(struct fast_index* ir, uint8_t* value, size_t mask_size);
int fast_index_get_next(struct fast_index* ir, uint8_t* value, size_t mask_size);

void fast_index_compare(struct fast_index* ir, const uint8_t* value);
void fast_index_compare_buffer(struct fast_index* ir, const uint8_t* buffer, size_t size);

int fast_index_intersect(struct fast_index* ir);
int fast_index_exclude(struct fast_index* ir);

void fast_index_clean(struct fast_index* ir);
void fast_index_delete(struct fast_index* ir);

#endif