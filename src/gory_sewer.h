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

struct gory_sewer_knob* gory_sewer_create(size_t chunk_size);

/* returns a pointer to the data in the sewer */
void* gory_sewer_alloc(struct gory_sewer_knob* gsk, size_t size);
void* gory_sewer_push(struct gory_sewer_knob* gsk, const void* ptr, size_t size);

void* gory_sewer_first(struct gory_sewer_knob* gsk);
void* gory_sewer_next(struct gory_sewer_knob* gsk);

void gory_sewer_delete(struct gory_sewer_knob* gsk);

/* Gory Sewer Size Layer */

void* gs_sl_alloc(struct gory_sewer_knob* gsk, size_t size);
void* gs_sl_push(struct gory_sewer_knob* gsk, const void* ptr, size_t size);

void* gs_sl_first(struct gory_sewer_knob* gsk, size_t* size);
void* gs_sl_next(struct gory_sewer_knob* gsk, size_t* size);

#endif
