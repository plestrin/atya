#ifndef SKIM_H
#define SKIM_H

#include "simple_set.h"
#include "gory_sewer.h"

struct skim {
	struct simple_set ss_index;
	struct gory_sewer_knob* gsk_data;
	size_t data_size; /* size of the data stored in the structure, not the memory consumption of this structure, not affected by shrink */
};

int skim_init(struct skim* sk);

int skim_add_data(struct skim* sk, const uint8_t* data, size_t size);

struct skim_iter {
	struct simple_set_iter ssi;
};

#define SKIM_ITER_INIT(sk) {.ssi = SIMPLE_SET_ITER_INIT_FIRST(&(sk)->ss_index)}

static inline void skim_iter_init(struct skim_iter* si, struct skim* sk){
	simple_set_iter_init_first(&si->ssi, &sk->ss_index);
}

static inline void skim_iter_next(struct skim_iter* si){
	simple_set_iter_next(&si->ssi);
}

int skim_iter_get(struct skim_iter* si, uint8_t** data_ptr, size_t* size_ptr);

void skim_delete_data(struct skim* sk, struct skim_iter* si);
void skim_shrink_data(struct skim_iter* si, size_t off_sta, size_t off_sto);

void skim_clean(struct skim* sk);
void skim_delete(struct skim* sk);

#endif
