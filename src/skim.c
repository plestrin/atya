#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "skim.h"

#define SKIM_MAX_DATA_SIZE 65536

struct skim_desc {
	uint8_t* ptr;
	size_t size;
};

int skim_init(struct skim* sk){
	simple_set_init(&sk->ss_index, sizeof(struct skim_desc));
	if ((sk->gsk_data = gory_sewer_create(SKIM_MAX_DATA_SIZE)) == NULL){
		fprintf(stderr, "[-] in %s, enable to init gory sewer\n", __func__);
		return -1;
	}

	sk->data_size = 0;

	return 0;
}

int skim_add_data(struct skim* sk, const uint8_t* data, size_t size){
	struct skim_desc d;

	if ((d.ptr = gory_sewer_push(sk->gsk_data, data, size)) == NULL){
		fprintf(stderr, "[-] in %s, unable to push data to the gory sewer\n", __func__);
		return -1;
	}

	d.size = size;
	if (simple_set_add_item(&sk->ss_index, &d)){
		fprintf(stderr, "[-] in %s, unable to push descriptor to the simple set\n", __func__);
		return -1;
	}

	sk->data_size += size;

	return 0;
}

int skim_iter_get(struct skim_iter* si, uint8_t** data_ptr, size_t* size_ptr){
	struct skim_desc* d;

	if ((d = simple_set_iter_get(&si->ssi)) == NULL){
		return -1;
	}

	*data_ptr = d->ptr;
	*size_ptr = d->size;

	return 0;
}

void skim_delete_data(struct skim* s, struct skim_iter* si){
	simple_set_delete_item(&s->ss_index, &si->ssi);
}

void skim_shrink_data(struct skim_iter* si, size_t off_sta, size_t off_sto){
	struct skim_desc* d;

	if ((d = simple_set_iter_get(&si->ssi)) != NULL){
		if (off_sta >= d->size){
			fprintf(stderr, "[-] in %s, off_sta is too big, abort\n", __func__);
			return;
		}

		d->ptr += off_sta;
		d->size -= off_sta;

		if (off_sto >= d->size){
			fprintf(stderr, "[-] in %s, off_sta is too big, abort\n", __func__);
			return;
		}
		d->size -= off_sto;
	}
}

void skim_clean(struct skim* sk){
	simple_set_clean(&sk->ss_index);
	gory_sewer_delete(sk->gsk_data);
	sk->gsk_data = NULL;
	sk->data_size = 0;
}

void skim_delete(struct skim* sk){
	skim_clean(sk);
	free(sk);
}
