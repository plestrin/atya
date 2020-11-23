#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

#include "skim.h"

#define SKIM_MAX_DATA_SIZE 65536

struct skim_desc {
	size_t off;
	size_t size;
};

void skim_init(struct skim* sk, uint8_t* data, size_t size){
	simple_set_init(&sk->ss_index, sizeof(struct skim_desc));
	sk->data = data;
	sk->size = size;
}

int skim_add_data(struct skim* sk, size_t off, size_t size){
	struct skim_desc d;

	d.off = off;
	d.size = size;

	if (simple_set_add_item(&sk->ss_index, &d)){
		fprintf(stderr, "[-] in %s, unable to push descriptor to the simple set\n", __func__);
		return -1;
	}

	return 0;
}

int skim_iter_get(struct skim_iter* ski, uint8_t** data_ptr, size_t* size_ptr){
	struct skim_desc* d;

	if ((d = simple_set_iter_get(&ski->ssi)) == NULL){
		return -1;
	}

	*data_ptr = ski->sk->data + d->off;
	*size_ptr = d->size;

	return 0;
}

void skim_delete_data(struct skim_iter* ski){
	simple_set_delete_item(&ski->sk->ss_index, &ski->ssi);
}

void skim_shrink_data(struct skim_iter* ski, size_t off_sta, size_t off_sto){
	struct skim_desc* d;

	if ((d = simple_set_iter_get(&ski->ssi)) != NULL){
		if (off_sta >= d->size){
			fprintf(stderr, "[-] in %s, off_sta is too big, abort\n", __func__);
			return;
		}

		d->off += off_sta;
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
	free(sk->data);
	sk->data = NULL;
	sk->size = 0;
}

void skim_delete(struct skim* sk){
	skim_clean(sk);
	free(sk);
}
