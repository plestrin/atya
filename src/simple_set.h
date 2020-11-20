#ifndef SIMPLE_SET_H
#define SIMPLE_SET_H

#include <stdlib.h>
#include <stdint.h>

#include "llist.h"

/* 	This structure (and its associated API) has the following functionalities / limitations:
		- fixed size item (defined at creation time) ;
		- access (linear - use iterator), add (constant time) and delete items (constant time) ;
		- items ordering is not preserved ;
		- do not save pointers to item stored in this structure nor index of any sort ;
	Compile time configuration :
		SIMPLE_SET_ALLOC_STEP : adjust the alloc step (number of items, default is 64)
*/

#ifndef SIMPLE_SET_ALLOC_STEP
#define SIMPLE_SET_ALLOC_STEP 64
#endif

struct simple_set_block {
	struct llist ll;
	uint64_t nb_item;
};

struct simple_set {
	size_t item_size;
	struct llist root;
	uint64_t nb_item;
};

static inline void simple_set_init(struct simple_set* ss, size_t item_size){
	ss->item_size = item_size;
	llist_init_circular(&ss->root);
	ss->nb_item = 0;
}

#define simple_set_add_item simple_set_push

int simple_set_push(struct simple_set* ss, const void* item);
void simple_set_pop(struct simple_set* ss);

struct simple_set_iter {
	struct simple_set* ss;
	struct simple_set_block* ssb;
	uint64_t index;
};

#define SIMPLE_SET_ITER_INIT_FIRST(ss_) {.ss = (ss_), .ssb = (struct simple_set_block*)(ss_)->root.next, .index = 0}
#define SIMPLE_SET_ITER_INIT_LAST(ss_) {.ss = (ss_), .ssb = (struct simple_set_block*)(ss_)->root.prev, .index = SIMPLE_SET_ALLOC_STEP}

static inline void simple_set_iter_init_first(struct simple_set_iter* ssi, struct simple_set* ss){
	ssi->ss = ss;
	ssi->ssb = (struct simple_set_block*)ss->root.next;
	ssi->index = 0;
}

static inline void simple_set_iter_init_last(struct simple_set_iter* ssi, struct simple_set* ss){
	ssi->ss = ss;
	ssi->ssb = (struct simple_set_block*)ss->root.prev;
	ssi->index = SIMPLE_SET_ALLOC_STEP;
}

void* simple_set_iter_get(struct simple_set_iter* ssi);

void simple_set_iter_next(struct simple_set_iter* ssi);
void simple_set_iter_prev(struct simple_set_iter* ssi);

void simple_set_delete_item(struct simple_set* ss, struct simple_set_iter* ssi);

void simple_set_clean(struct simple_set* ss);

#endif
