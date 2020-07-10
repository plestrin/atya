#ifndef LAST_INDEX_H
#define LAST_INDEX_H

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>

struct last_entry {
	size_t alloc_size;
	size_t used_size;
	uint64_t nb_item;
	uint8_t* ptr;
};

struct last_index {
	size_t min_size;
	size_t max_size;
	uint64_t nb_item;
	struct last_entry* index[0x10000];
};

void last_index_init(struct last_index* li, size_t min_size);

int last_index_push(struct last_index* li, const uint8_t* data, size_t size);

void last_index_exclude_buffer(struct last_index* li, const uint8_t* buffer, size_t size, size_t overlap);
int last_index_exclude_file(struct last_index* li, const char* file_name);

void last_index_dump(struct last_index* li, FILE* stream);

void last_index_clean(struct last_index* li);

#endif
