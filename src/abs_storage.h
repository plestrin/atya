#ifndef ABS_STORAGE_H
#define ABS_STORAGE_H

#include <stdint.h>

#include "skim.h"
#include "gory_sewer.h"
#include "simple_index.h"
#include "stree.h"

struct abs_storage_file {
	uint8_t* data;
	struct skim sk;
};

struct abs_storage {
	uint64_t nb_file;
	struct abs_storage_file* asf_buffer;
};

int abs_storage_init(struct abs_storage* as, struct gory_sewer_knob* gsk_path);

int abs_storage_fill_simple_index_head(struct abs_storage* as, struct simple_index* si);

int abs_storage_intersect_simple_index_tail(struct abs_storage* as, struct simple_index* si);

int abs_storage_fill_stree_head(struct abs_storage* as, struct stree* tree, struct simple_index* si);

int abs_storage_intersect_stree_tail(struct abs_storage* as, struct stree* tree, struct simple_index* si);

void abs_storage_clean(struct abs_storage* as);

#endif
