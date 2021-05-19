#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>

#include "gory_sewer.h"

struct gory_sewer_first {
	void* next;
	size_t rem_size;
};

struct gory_sewer_knob* gory_sewer_create(size_t chunk_size){
	size_t first_chunk_size;
	uint8_t* chunk;
	struct gory_sewer_knob* gsk;
	struct gory_sewer_first* gsf;

	first_chunk_size = chunk_size;
	if (first_chunk_size < sizeof(struct gory_sewer_knob) + sizeof(struct gory_sewer_first)){
		first_chunk_size = sizeof(struct gory_sewer_knob) + sizeof(struct gory_sewer_first);
	}

	if ((chunk = malloc(first_chunk_size)) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return NULL;
	}

	gsk = (struct gory_sewer_knob*)chunk;
	gsk->head = NULL;
	gsk->tail = NULL;
	gsk->curr = NULL;
	gsk->chunk_size = (chunk_size < sizeof(struct gory_sewer_first)) ? sizeof(struct gory_sewer_first) : chunk_size;
	gsk->nb_item = 0;

	gsf = (struct gory_sewer_first*)(gsk + 1);
	gsf->next = NULL;
	gsf->rem_size = first_chunk_size - sizeof(struct gory_sewer_knob) - sizeof(void*);

	return gsk;
}

static void* gory_sewer_alloc_std(struct gory_sewer_knob* gsk, size_t size){
	struct gory_sewer_first* gsf;
	struct gory_sewer_first* new_gsf;
	size_t rem_size;

	if (gsk->head == NULL){
		gsf = (struct gory_sewer_first*)(gsk + 1);
	}
	else {
		gsf = *gsk->head;
	}

	rem_size = gsf->rem_size;
	if (rem_size < size){
		if ((gsf = malloc(gsk->chunk_size)) == NULL){
			printf("[-] in %s, unable to allocate memory\n", __func__);
			return NULL;
		}
		rem_size = gsk->chunk_size - sizeof(void*);
		if (gsk->head != NULL){
			*gsk->head = gsf;
		}
	}

	rem_size -= size;

	if (rem_size < sizeof(struct gory_sewer_first)){
		if ((new_gsf = malloc(gsk->chunk_size)) == NULL){
			printf("[-] in %s, unable to allocate memory\n", __func__);
			return NULL;
		}
		rem_size = gsk->chunk_size;
	}
	else {
		new_gsf = (struct gory_sewer_first*)((uint8_t*)gsf + sizeof(void*) + size);
	}

	gsf->next = new_gsf;

	new_gsf->next = NULL;
	new_gsf->rem_size = rem_size - sizeof(void*);

	return gsf;
}

static void* gory_sewer_alloc_ext(struct gory_sewer_knob* gsk, size_t size){
	void* raw;

	if ((raw = malloc(sizeof(void*) + size)) == NULL){
		printf("[-] in %s, unable to allocate memory\n", __func__);
		return NULL;
	}

	if (gsk->head == NULL){
		*(void**)raw = gsk + 1;
	}
	else {
		*(void**)raw = *gsk->head;
		*gsk->head = raw;
	}

	return raw;
}

void* gory_sewer_alloc(struct gory_sewer_knob* gsk, size_t size){
	void* raw;

	if (size >= gsk->chunk_size - sizeof(void*)){
		raw = gory_sewer_alloc_ext(gsk, size);
	}
	else {
		raw = gory_sewer_alloc_std(gsk, size);
	}

	if (raw == NULL){
		return raw;
	}

	gsk->head = raw;
	if (gsk->tail == NULL){
		gsk->tail = raw;
	}

	gsk->nb_item ++;

	return (uint8_t*)raw + sizeof(void*);
}

void* gory_sewer_push(struct gory_sewer_knob* gsk, const void* ptr, size_t size){
	void* dst;

	if ((dst = gory_sewer_alloc(gsk, size)) != NULL){
		memcpy(dst, ptr, size);
	}

	return dst;
}

void* gory_sewer_first(struct gory_sewer_knob* gsk){
	if (gsk->tail == NULL){
		return NULL;
	}
	gsk->curr = *gsk->tail;
	return (uint8_t*)gsk->tail + sizeof(void*);
}

void* gory_sewer_next(struct gory_sewer_knob* gsk){
	void* result;

	if (*gsk->curr == NULL){
		return NULL;
	}
	result = (uint8_t*)gsk->curr + sizeof(void*);
	gsk->curr = *gsk->curr;
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

void* gs_sl_alloc(struct gory_sewer_knob* gsk, size_t size){
	void* raw;

	if ((raw = gory_sewer_alloc(gsk, size + sizeof(size_t))) == NULL){
		return NULL;
	}

	*(size_t*)raw = size;
	return (uint8_t*)raw + sizeof(size_t);
}

void* gs_sl_push(struct gory_sewer_knob* gsk, const void* ptr, size_t size){
	void* dst;

	if ((dst = gs_sl_alloc(gsk, size)) != NULL){
		memcpy(dst, ptr, size);
	}

	return dst;
}

void* gs_sl_first(struct gory_sewer_knob* gsk, size_t* size){
	void* raw;

	if ((raw = gory_sewer_first(gsk)) == NULL){
		return NULL;
	}

	*size = *(size_t*)raw;
	return (uint8_t*)raw + sizeof(size_t);
}

void* gs_sl_next(struct gory_sewer_knob* gsk, size_t* size){
	void* raw;

	if ((raw = gory_sewer_next(gsk)) == NULL){
		return NULL;
	}

	*size = *(size_t*)raw;
	return (uint8_t*)raw + sizeof(size_t);
}
