#ifndef INDEX_H
#define INDEX_H

#include <stdlib.h>
#include <stdint.h>

struct index_root {
	size_t size;
	void* root[0x10000];
};

int index_root_do_insert(struct index_root* ir, const uint8_t* value);
int index_root_do_insert_buffer(struct index_root* ir, const uint8_t* buffer, size_t size);

int index_root_do_test(struct index_root* ir, const uint8_t* value);

uint64_t index_root_do_count(struct index_root* ir);

int index_root_do_get_first(struct index_root* ir, uint8_t* value, size_t mask_size);
int index_root_do_get_next(struct index_root* ir, uint8_t* value, size_t mask_size);

void index_root_do_compare(struct index_root* ir, const uint8_t* value);
void index_root_do_compare_buffer(struct index_root* ir, const uint8_t* buffer, size_t size);

int index_root_do_intersect(struct index_root* ir);
int index_root_do_exclude(struct index_root* ir);

void index_root_do_clean(struct index_root* ir);
void index_root_do_delete(struct index_root* ir);

#endif
