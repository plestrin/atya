#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "skim.h"

#define SKIM_MAX_DATA_SIZE 65536

struct skim_desc {
	size_t off;
	size_t size;
};

int skim_init(struct skim* sk, uint8_t* data, size_t size){
	struct skim_desc d;

	simple_set_init(&sk->ss_index, sizeof(struct skim_desc));

	sk->data = data;
	sk->size = size;

	d.off = 0;
	d.size = size;

	return simple_set_add_item(&sk->ss_index, &d);
}

int skim_add_data(struct skim* sk, size_t off, size_t size){
	struct skim_desc d;

	d.off = off;
	d.size = size;

	return simple_set_add_item(&sk->ss_index, &d);
}

int skim_iter_get(struct skim_iter* ski, size_t* off_ptr, size_t* size_ptr){
	struct skim_desc* d;

	if ((d = simple_set_iter_get(&ski->ssi)) == NULL){
		return -1;
	}

	*off_ptr = d->off;
	*size_ptr = d->size;

	return 0;
}

int skim_resize_data(struct skim_iter* ski, size_t new_off, size_t new_size){
	struct skim_desc* d;

	if ((d = simple_set_iter_get(&ski->ssi)) == NULL){
		return -1;
	}

	d->off = new_off;
	d->size = new_size;

	return 0;
}

void skim_clean(struct skim* sk){
	simple_set_clean(&sk->ss_index);
	free(sk->data);
	sk->data = NULL;
	sk->size = 0;
}

void skim_delete(struct skim* sk){
	skim_clean(sk);
	free(sk);
}
