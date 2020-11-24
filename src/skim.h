#ifndef SKIM_H
#define SKIM_H

#include "simple_set.h"

struct skim {
	struct simple_set ss_index;
	uint8_t* data;
	size_t size;
};

int skim_init(struct skim* sk, uint8_t* data, size_t size);

int skim_add_data(struct skim* sk, size_t off, size_t size);

struct skim_iter {
	struct simple_set_iter ssi;
	struct skim* sk;
};

#define SKIM_ITER_INIT(sk_) {.ssi = SIMPLE_SET_ITER_INIT_FIRST(&(sk_)->ss_index), .sk = (sk_)}

static inline void skim_iter_init(struct skim_iter* ski, struct skim* sk){
	simple_set_iter_init_first(&ski->ssi, &sk->ss_index);
	ski->sk = sk;
}

static inline void skim_iter_next(struct skim_iter* ski){
	simple_set_iter_next(&ski->ssi);
}

int skim_iter_get(struct skim_iter* ski, size_t* off_ptr, size_t* size_ptr);

int skim_resize_data(struct skim_iter* ski, size_t new_off, size_t new_size);

static inline void skim_delete_data(struct skim_iter* ski){
	simple_set_delete_item(&ski->sk->ss_index, &ski->ssi);
}


void skim_clean(struct skim* sk);
void skim_delete(struct skim* sk);

#endif
