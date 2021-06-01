#ifndef ABS_INDEX_H
#define ABS_INDEX_H

#include <stdlib.h>
#include <stdint.h>

#include "fast_index.h"
#include "simple_index.h"

struct abs_fast_index {
	struct fast_index* fi;
};

struct abs_simple_index {
	struct simple_index* si;
	uint16_t hash;
};

union union_index {
	struct abs_simple_index asi;
	struct abs_fast_index afi;
};

struct abs_index {
	union union_index ui;

	size_t (*index_get_size)(union union_index* ui);

	uint64_t (*index_compare_init)(union union_index* ui, const uint8_t* data);
	uint64_t (*index_compare_next)(union union_index* ui, const uint8_t* data); // compare with data + 1

	uint64_t (*index_compare_buffer)(union union_index* ui, const uint8_t* buffer, size_t size);

	uint64_t (*index_remove_hit)(union union_index* ui);
	uint64_t (*index_remove_nohit)(union union_index* ui);
	uint64_t (*index_remove_nohitpro)(union union_index* ui);

	void (*index_clean)(union union_index* ui);
};

void abs_index_init_simple(struct abs_index* ai, struct simple_index* si);
void abs_index_init_fast(struct abs_index* ai, struct fast_index* fi);

static inline size_t abs_index_get_size(struct abs_index* ai){
	return ai->index_get_size(&ai->ui);
}

static inline uint64_t abs_index_compare_init(struct abs_index* ai, const uint8_t* data){
	return ai->index_compare_init(&ai->ui, data);
}

static inline uint64_t abs_index_compare_next(struct abs_index* ai, const uint8_t* data){
	return ai->index_compare_next(&ai->ui, data);
}

static inline uint64_t abs_index_compare_buffer(struct abs_index* ai, const uint8_t* buffer, size_t size){
	return ai->index_compare_buffer(&ai->ui, buffer, size);
}

static inline uint64_t abs_index_remove_hit(struct abs_index* ai){
	return ai->index_remove_hit(&ai->ui);
}

static inline uint64_t abs_index_remove_nohit(struct abs_index* ai){
	return ai->index_remove_nohit(&ai->ui);
}

static inline uint64_t abs_index_remove_nohitpro(struct abs_index* ai){
	return ai->index_remove_nohitpro(&ai->ui);
}

static inline void abs_index_clean(struct abs_index* ai){
	ai->index_clean(&ai->ui);
}

#endif
