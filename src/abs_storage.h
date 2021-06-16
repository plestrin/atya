#ifndef ABS_STORAGE_H
#define ABS_STORAGE_H

#include <stdint.h>

#include "skim.h"
#include "gory_sewer.h"
#include "simple_index.h"
#include "stree.h"

#define ABS_STORAGE_FILE_FLAG_SK 1

struct abs_storage_file {
	char* path;
	uint64_t size;
	uint64_t flags;
	struct skim sk;
};

struct abs_storage {
	uint64_t nb_file;
	uint64_t size;
	struct abs_storage_file* asf_buffer;
};

int abs_storage_init(struct abs_storage* as, struct gory_sewer_knob* gsk_path);

int abs_storage_fill_simple_index_head(struct abs_storage* as, struct simple_index* si);

int abs_storage_intersect_simple_index_tail(struct abs_storage* as, struct simple_index* si);

int abs_storage_fill_stree_head(struct abs_storage* as, struct stree* tree, struct simple_index* si);

int abs_storage_intersect_stree_tail(struct abs_storage* as, struct stree* tree, struct simple_index* si);

/* does not clean the GSK */
void abs_storage_clean(struct abs_storage* as);

#endif
