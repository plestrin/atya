#include "llist.h"

void llist_insert_before(struct llist* ll, struct llist* new_ll){
	new_ll->next = ll;
	new_ll->prev = ll->prev;
	if (ll->prev != NULL){
		ll->prev->next = new_ll;
	}
	ll->prev = new_ll;
}

void llist_insert_after(struct llist* ll, struct llist* new_ll){
	new_ll->next = ll->next;
	new_ll->prev = ll;
	if (ll->next != NULL){
		ll->next->prev = NULL;
	}
	ll->next = new_ll;
}

void llist_splice_before(struct llist* ll, struct llist* new_ll){
	struct llist* ll_cursor;

	ll_cursor = llist_get_tail(new_ll);
	ll_cursor->prev = ll->prev;
	if (ll->prev != NULL){
		ll->prev->next = ll_cursor;
	}

	ll_cursor = llist_get_head(new_ll);
	ll_cursor->next = ll;
	ll->prev = ll_cursor;
}

void llist_splice_after(struct llist* ll, struct llist* new_ll){
	struct llist* ll_cursor;

	ll_cursor = llist_get_head(new_ll);
	ll_cursor->next = ll->next;
	if (ll->next != NULL){
		ll->next->prev = ll_cursor;
	}

	ll_cursor = llist_get_tail(new_ll);
	ll_cursor->prev = ll;
	ll->next = ll_cursor;
}

void llist_remove(struct llist* ll){
	if (ll->prev != NULL){
		ll->prev->next = ll->next;
	}
	if (ll->next != NULL){
		ll->next->prev = ll->prev;
	}

	ll->prev = NULL;
	ll->next = NULL;
}
