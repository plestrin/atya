#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "last_index.h"
#include "hash.h"

struct last_entry_item {
	size_t size;
	const uint8_t* data;
};

#define DEFAULT_ITEM_PER_ENTRY 64

static inline struct last_entry_item* last_entry_get_item_buffer(struct last_entry* le){
	return (struct last_entry_item*)(le + 1);
}

static int last_entry_push(struct last_entry** le_ptr, const uint8_t* data, size_t size){
	struct last_entry* le;
	uint64_t nb_alloc = 0;
	uint64_t nb_item = 0;
	struct last_entry_item* lei_buffer;

	le = *le_ptr;

	if (le != NULL){
		nb_alloc = le->nb_alloc;
		nb_item = le->nb_item;
	}

	if (le->nb_alloc == le->nb_item){
		nb_alloc += DEFAULT_ITEM_PER_ENTRY;

		if ((le = realloc(le, sizeof(struct last_entry) + nb_alloc * sizeof(struct last_entry_item))) == NULL){
			fprintf(stderr, "[-] in %s, unable to realloc memory\n", __func__);
			return ENOMEM;
		}

		le->nb_alloc = nb_alloc;
		le->nb_item = nb_item;

		*le_ptr = le;
	}

	lei_buffer = last_entry_get_item_buffer(le);

	lei_buffer[le->nb_item].size = size;
	lei_buffer[le->nb_item].data = data;

	le->nb_item ++;

	return 0;
}

static int last_entry_exclude(struct last_entry* le, const void* data, size_t size){
	struct last_entry_item* lei_buffer;
	uint64_t i;

	lei_buffer = last_entry_get_item_buffer(le);

	for (i = 0; i < le->nb_item; i ++){
		if (lei_buffer[i].size <= size && !memcmp(lei_buffer[i].data, data, lei_buffer[i].size)){
			le->nb_item --;
			if (i < le->nb_item){
				memcpy(lei_buffer + i, lei_buffer + le->nb_item, sizeof(struct last_entry_item));
			}
			return 1;
		}
	}

	return 0;
}

static void last_entry_dump(struct last_entry* le, FILE* stream){
	struct last_entry_item* lei_buffer;
	uint64_t i;
	uint64_t size;

	lei_buffer = last_entry_get_item_buffer(le);

	for (i = 0; i < le->nb_item; i ++){
		size = lei_buffer[i].size;
		fwrite(&size, sizeof size, 1, stream);
		fwrite(lei_buffer[i].data, lei_buffer[i].size, 1, stream);
	}
}

void last_index_init(struct last_index* li, size_t min_size){
	li->min_size = min_size;
	li->max_size = 0;
	li->nb_item = 0;
	memset(li->index, 0, sizeof li->index);
}

int last_index_push(struct last_index* li, const uint8_t* data, size_t size){
	uint16_t hash;
	int status;

	if (size < li->min_size){
		fprintf(stderr, "[-] in %s, data is too small\n", __func__);
		return EINVAL;
	}

	hash = hash_init(data, li->min_size);
	if ((status = last_entry_push(li->index + hash, data, size))){
		return status;
	}

	if (size > li->max_size){
		li->max_size = size;
	}

	li->nb_item ++;

	return 0;
}

static void last_index_exclude_buffer(struct last_index* li, const uint8_t* buffer, size_t size, size_t overlap){
	uint16_t hash;
	size_t offset;

	if (overlap > size){
		return;
	}

	hash = hash_init(buffer, li->min_size);

	for (offset = 0; ; offset ++){
		if (li->index[hash] != NULL){
			if (last_entry_exclude(li->index[hash], buffer + offset, size - offset)){
				if (!li->index[hash]->nb_item){
					free(li->index[hash]);
					li->index[hash] = NULL;
				}
				li->nb_item --;
			}
		}

		if (offset + overlap >= size){
			break;
		}

		hash = hash_update(hash, buffer[offset], buffer[offset + li->min_size], li->min_size);
	}
}

static uint8_t chunk[0x40000];

int last_index_exclude_file(struct last_index* li, const char* file_name){
	FILE* handle;
	int status;
	size_t rd_sz;
	size_t da_sz;

	if (li->max_size >= sizeof chunk){
		fprintf(stderr, "[-] in %s, chunk is too small to handle item of size 0x%zx\n", __func__, li->max_size);
		return EINVAL;
	}

	if ((handle = fopen(file_name, "rb")) == NULL){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, file_name);
		return status;
	}

	for (rd_sz = fread(chunk, 1, sizeof chunk, handle), da_sz = rd_sz; da_sz == sizeof chunk; da_sz = rd_sz + li->max_size - 1){
		last_index_exclude_buffer(li, chunk, sizeof chunk, li->max_size);
		memmove(chunk, chunk + sizeof chunk - li->max_size + 1, li->max_size - 1);
		rd_sz = fread(chunk + li->max_size - 1, 1, sizeof chunk - li->max_size + 1, handle);
	}

	last_index_exclude_buffer(li, chunk, da_sz, li->min_size);

	fclose(handle);

	return 0;
}

void last_index_dump_and_clean(struct last_index* li, FILE* stream){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		if (li->index[i] != NULL){
			last_entry_dump(li->index[i], stream);
			free(li->index[i]);
			li->index[i] = NULL;
		}
	}
}
