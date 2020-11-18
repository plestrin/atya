#ifndef GORY_SEWER_H
#define GORY_SEWER_H

#include <stdlib.h>
#include <stdint.h>

struct gory_sewer_knob {
	void** head;
	void** tail;
	void** curr;
	size_t chunk_size;
	uint64_t nb_item;
};

/* chunk size should be larger than the largest item */
struct gory_sewer_knob* gory_sewer_create(size_t chunk_size);

int gory_sewer_push(struct gory_sewer_knob* gsk, const void* ptr, size_t size);

void* gory_sewer_first(struct gory_sewer_knob* gsk);
void* gory_sewer_next(struct gory_sewer_knob* gsk);

void gory_sewer_delete(struct gory_sewer_knob* gsk);

#endif
