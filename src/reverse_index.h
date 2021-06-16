#ifndef REVERSE_INDEX_H
#define REVERSE_INDEX_H

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#include "gory_sewer.h"

struct reverse_entry_item {
	const uint8_t* ptr;
	size_t size;
};

struct reverse_entry {
	uint32_t nb_alloc;
	uint32_t nb_item;
	struct reverse_entry_item items[];
};

struct reverse_index {
	struct gory_sewer_knob* gsk;
	size_t min_size;
	struct reverse_entry* index[0x10000];
};

int reverse_index_init(struct reverse_index* ri, size_t min_size);

struct reverse_index* reverse_index_create(size_t min_size);

int reverse_index_append(struct reverse_index* ri, const uint8_t* ptr, size_t size);

void reverse_index_dump(struct reverse_index* ri, FILE* stream);

void reverse_index_clean(struct reverse_index* ri);

static inline void reverse_index_delete(struct reverse_index* ri){
	reverse_index_clean(ri);
	free(ri);
}

#endif
