#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>

#include "gory_sewer.h"

struct gory_sewer_last {
	void* next;
	size_t rem_size;
};

struct gory_sewer_knob* gory_sewer_create(size_t chunk_size){
	size_t first_chunk_size;
	uint8_t* chunk;
	struct gory_sewer_knob* gsk;
	struct gory_sewer_last* gsl;

	first_chunk_size = chunk_size;
	if (first_chunk_size < sizeof(struct gory_sewer_knob) + sizeof(struct gory_sewer_last)){
		first_chunk_size = sizeof(struct gory_sewer_knob) + sizeof(struct gory_sewer_last);
	}

	if ((chunk = malloc(first_chunk_size)) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return NULL;
	}

	gsk = (struct gory_sewer_knob*)chunk;
	gsk->head = NULL;
	gsk->tail = NULL;
	gsk->curr = NULL;
	gsk->chunk_size = (chunk_size < sizeof(struct gory_sewer_last)) ? sizeof(struct gory_sewer_last) : chunk_size;
	gsk->nb_item = 0;

	gsl = (struct gory_sewer_last*)(gsk + 1);
	gsl->next = NULL;
	gsl->rem_size = first_chunk_size - sizeof(struct gory_sewer_knob) - sizeof(void*);

	return gsk;
}

int gory_sewer_push(struct gory_sewer_knob* gsk, const void* ptr, size_t size){
	struct gory_sewer_last* gsl;
	struct gory_sewer_last* new_gsl;
	size_t rem_size;

	if (size > gsk->chunk_size + sizeof(void*)){
		printf("[-] in %s, item size is larger than chunk size\n", __func__);
		return EINVAL;
	}

	if (gsk->head == NULL){
		gsl = (struct gory_sewer_last*)(gsk + 1);
	}
	else {
		gsl = *((void**)gsk->head);
	}

	rem_size = gsl->rem_size;
	if (rem_size < size){
		if ((gsl = malloc(gsk->chunk_size)) == NULL){
			printf("[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}
		rem_size = gsk->chunk_size - sizeof(void*);
		if (gsk->head != NULL){
			*(void**)gsk->head = gsl;
		}
	}

	rem_size -= size;

	if (rem_size < sizeof(struct gory_sewer_last)){
		if ((new_gsl = malloc(gsk->chunk_size)) == NULL){
			printf("[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}
		rem_size = gsk->chunk_size;
	}
	else {
		new_gsl = (struct gory_sewer_last*)((uint8_t*)gsl + sizeof(void*) + size);
	}

	memcpy((uint8_t*)gsl + sizeof(void*), ptr, size);

	if (gsk->tail == NULL){
		gsk->tail = (void**)gsl;
	}
	gsk->head = (void**)gsl;
	gsl->next = new_gsl;

	gsk->nb_item ++;

	new_gsl->next = NULL;
	new_gsl->rem_size = rem_size - sizeof(void*);

	return 0;
}

void* gory_sewer_first(struct gory_sewer_knob* gsk){
	if (gsk->tail == NULL){
		return NULL;
	}
	gsk->curr = *(void**)gsk->tail;
	return (uint8_t*)gsk->tail + sizeof(void*);
}

void* gory_sewer_next(struct gory_sewer_knob* gsk){
	void* result;

	if (*(void**)gsk->curr == NULL){
		return NULL;
	}
	result = (uint8_t*)gsk->curr + sizeof(void*);
	gsk->curr = *(void**)gsk->curr;
	return result;
}

void gory_sewer_delete(struct gory_sewer_knob* gsk){
	uint8_t* addr_prev;
	uint8_t* addr_next;
	size_t size;

	addr_prev = (uint8_t*)gsk;
	size = malloc_usable_size(gsk);

	for (addr_next = gory_sewer_first(gsk); addr_next != NULL; addr_next = gory_sewer_next(gsk)){
		addr_next -= sizeof(void*);
		if (addr_next < addr_prev || addr_next >= addr_prev + size){
			if (addr_prev != (uint8_t*)gsk){
				free(addr_prev);
			}
			addr_prev = addr_next;
			size = malloc_usable_size(addr_prev);
		}
	}

	if (addr_prev != (uint8_t*)gsk){
		free(addr_prev);
	}
	free(gsk);
}
