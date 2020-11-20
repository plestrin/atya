#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "simple_set.h"

static struct simple_set_block* simple_set_block_create(size_t item_size){
	struct simple_set_block* ssb;

	if ((ssb = malloc(sizeof(struct simple_set_block) * item_size * SIMPLE_SET_ALLOC_STEP)) != NULL){
		ssb->nb_item = 0;
	}

	return ssb;
}

static inline struct simple_set_block* simple_set_block_get_next(struct simple_set_block* ssb){
	return (struct simple_set_block*)ssb->ll.next;
}

static inline struct simple_set_block* simple_set_block_get_prev(struct simple_set_block* ssb){
	return (struct simple_set_block*)ssb->ll.prev;
}

static inline uint8_t* simple_set_block_get_data(struct simple_set_block* ssb){
	return (uint8_t*)(ssb + 1);
}

int simple_set_push(struct simple_set* ss, const void* item){
	struct simple_set_block* ssb;

	ssb = (struct simple_set_block*)ss->root.prev;
	if (ss->root.prev == &ss->root || ssb->nb_item == SIMPLE_SET_ALLOC_STEP){
		if ((ssb = simple_set_block_create(ss->item_size)) == NULL){
			return -1;
		}
		llist_insert_before(&ss->root, &ssb->ll);
	}

	memcpy(simple_set_block_get_data(ssb) + ssb->nb_item * ss->item_size, item, ss->item_size);

	ssb->nb_item ++;
	ss->nb_item ++;

	return 0;
}

void simple_set_pop(struct simple_set* ss){
	struct simple_set_block* ssb;

	if (ss->root.prev == &ss->root){
		return;
	}

	ssb = (struct simple_set_block*)ss->root.prev;
	if (ssb->nb_item == 1){
		llist_remove(&ssb->ll);
		free(ssb);
	}
	else {
		ssb->nb_item --;
	}

	ss->nb_item --;

	return;
}

void* simple_set_iter_get(struct simple_set_iter* ssi){
	if ((struct llist*)ssi->ssb == &ssi->ss->root){
		return NULL;
	}
	if (ssi->index >= ssi->ssb->nb_item){
		if (!ssi->ssb->nb_item){
			return NULL;
		}
		ssi->index = ssi->ssb->nb_item - 1;
	}

	return simple_set_block_get_data(ssi->ssb) + ssi->index * ssi->ss->item_size;
}

void simple_set_iter_next(struct simple_set_iter* ssi){
	if ((struct llist*)ssi->ssb != &ssi->ss->root){
		ssi->index ++;
		if (ssi->index >= ssi->ssb->nb_item){
			ssi->index = 0;
			ssi->ssb = simple_set_block_get_next(ssi->ssb);
		}
	}
}

void simple_set_iter_prev(struct simple_set_iter* ssi){
	if ((struct llist*)ssi->ssb != &ssi->ss->root){
		if (!ssi->index){
			ssi->ssb = simple_set_block_get_prev(ssi->ssb);
			ssi->index = SIMPLE_SET_ALLOC_STEP;
		}
		else {
			ssi->index --;
		}
	}
}

void simple_set_delete_item(struct simple_set* ss, struct simple_set_iter* ssi){
	struct simple_set_iter ssil = SIMPLE_SET_ITER_INIT_LAST(ss);
	void* ptr_delete;
	void* ptr_last;

	if ((ptr_delete = simple_set_iter_get(ssi)) == NULL){
		return;
	}

	ptr_last = simple_set_iter_get(&ssil);
	if (ptr_last == ptr_delete){
		ssi->ssb = (struct simple_set_block*)&ss->root;
	}
	else {
		memcpy(ptr_delete, ptr_last, ss->item_size);
	}

	simple_set_pop(ss);
}

void simple_set_clean(struct simple_set* ss){
	struct llist* curr;
	struct llist* next;

	for (curr = ss->root.next; curr != &ss->root; curr = next){
		next = curr->next;
		free(curr);
	}

	simple_set_init(ss, ss->item_size);
}
