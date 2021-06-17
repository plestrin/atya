#ifndef LLIST_H
#define LLIST_H

#include <stdlib.h>
#include <stdint.h>

struct llist {
	struct llist* prev;
	struct llist* next;
};

#define llist_get_data(ll) ((void*)((struct llist*)(ll) + 1))

#define LLIST_NULL ((void*)((char*)NULL + sizeof(struct llist)))

#define LLIST_INIT {.prev = NULL, .next = NULL}

static inline void llist_init(struct llist* ll){
	ll->prev = NULL;
	ll->next = NULL;
}

static inline void llist_init_circular(struct llist* ll){
	ll->prev = ll;
	ll->next = ll;
}

static inline struct llist* llist_get_head(struct llist* ll){
	struct llist* ll_cursor;

	for (ll_cursor = ll; ll_cursor->next != NULL; ll_cursor = ll_cursor->next){
		if (ll_cursor->next == ll){
			return ll;
		}
	}

	return ll_cursor;
}

static inline struct llist* llist_get_tail(struct llist* ll){
	struct llist* ll_cursor;

	for (ll_cursor = ll; ll_cursor->prev != NULL; ll_cursor = ll_cursor->prev){
		if (ll_cursor->prev == ll){
			return ll;
		}
	}

	return ll_cursor;
}

/* new_ll is a single element, not necessarily initialized */
void llist_insert_before(struct llist* ll, struct llist* new_ll);
void llist_insert_after(struct llist* ll, struct llist* new_ll);
/* new_ll is properly initialized, may contain multiple elements */
void llist_splice_before(struct llist* ll, struct llist* new_ll);
void llist_splice_after(struct llist* ll, struct llist* new_ll);

static inline uint64_t llist_get_nb_element_before(struct llist* ll){
	uint64_t nb_element;

	for (nb_element = 0; ll->prev != NULL; ll = ll->prev, nb_element ++);

	return nb_element;
}

static inline uint64_t llist_get_nb_element_after(struct llist* ll){
	uint64_t nb_element;

	for (nb_element = 0; ll->next != NULL; ll = ll->next, nb_element ++);

	return nb_element;
}

static inline uint64_t get_nb_element(struct llist* ll){
	if (ll == NULL){
		return 0;
	}

	return 1 + llist_get_nb_element_before(ll) + llist_get_nb_element_after(ll);
}

void llist_remove(struct llist* ll);

#endif
