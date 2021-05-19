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

static struct last_entry* last_entry_push(struct last_entry* le, const uint8_t* data, size_t size){
	struct last_entry* new_le;
	uint64_t nb_alloc = 0;
	uint64_t nb_used = 0;
	struct last_entry_item* lei_buffer;

	if (le != NULL){
		nb_alloc = le->nb_alloc;
		nb_used = le->nb_used;
	}

	if (le->nb_alloc == le->nb_used){
		nb_alloc += DEFAULT_ITEM_PER_ENTRY;

		if ((new_le = realloc(le, sizeof(struct last_entry) + nb_alloc * sizeof(struct last_entry_item))) == NULL){
			fprintf(stderr, "[-] in %s, unable to realloc memory\n", __func__);
			return NULL;
		}

		le = new_le;
		le->nb_alloc = nb_alloc;
		le->nb_used = nb_used;
	}

	lei_buffer = last_entry_get_item_buffer(le);

	lei_buffer[le->nb_used].size = size;
	lei_buffer[le->nb_used].data = data;

	le->nb_used ++;

	return le;
}

static uint32_t last_entry_exclude(struct last_entry* le, const void* data, size_t size){
	struct last_entry_item* lei_buffer;
	uint64_t i;

	lei_buffer = last_entry_get_item_buffer(le);

	for (i = 0; i < le->nb_used; i ++){
		if (lei_buffer[i].size <= size && !memcmp(lei_buffer[i].data, data, lei_buffer[i].size)){
			le->nb_used --;
			if (i < le->nb_used){
				memcpy(lei_buffer + i, lei_buffer + le->nb_used, sizeof(struct last_entry_item));
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

	for (i = 0; i < le->nb_used; i ++){
		size = lei_buffer[i].size;
		fwrite(&size, sizeof size, 1, stream);
		fwrite(lei_buffer[i].data, lei_buffer[i].size, 1, stream);
	}
}

void last_index_init(struct last_index* li, size_t min_size){
	li->min_size = min_size;
	li->max_size = 0;
	memset(li->index, 0, sizeof li->index);
}

int last_index_push(struct last_index* li, const uint8_t* data, size_t size){
	uint16_t hash;
	struct last_entry* le;

	if (size < li->min_size){
		fprintf(stderr, "[-] in %s, size argument is too small\n", __func__);
		return EINVAL;
	}

	hash = hash_init(data, li->min_size);
	if ((le = last_entry_push(li->index[hash], data, size)) == NULL){
		return ENOMEM;
	}

	li->index[hash] = le;
	li->nb_item ++;
	if (size > li->max_size){
		li->max_size = size;
	}

	return 0;
}

static void last_index_exclude_buffer(struct last_index* li, const uint8_t* buffer, size_t size, size_t overlap){
	uint16_t hash;
	size_t offset;

	for (offset = 0; offset + overlap <= size; offset ++){
		hash = hash_init(buffer + offset, li->min_size);
		if (li->index[hash] != NULL){
			li->nb_item -= last_entry_exclude(li->index[hash], buffer + offset, size - offset);
			if (!li->index[hash]->nb_used){
				free(li->index[hash]);
				li->index[hash] = NULL;
			}
		}
	}
}

static uint8_t chunk[0x10000];

int last_index_exclude_file(struct last_index* li, const char* file_name){
	FILE* handle;
	int status;
	size_t rd_sz;

	if (li->max_size >= sizeof chunk){
		fprintf(stderr, "[-] in %s, chunk is too small to handle item of size 0x%zx\n", __func__, li->max_size);
		return EINVAL;
	}

	if ((handle = fopen(file_name, "rb")) == NULL){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, file_name);
		return status;
	}

	for (rd_sz = fread(chunk, 1, sizeof chunk, handle); ; ){
		if (rd_sz < sizeof chunk){
			last_index_exclude_buffer(li, chunk, rd_sz, li->min_size);
			break;
		}

		last_index_exclude_buffer(li, chunk, rd_sz, li->max_size);
		memmove(chunk, chunk + rd_sz - li->max_size + 1, li->max_size - 1);
		rd_sz = fread(chunk + li->max_size - 1, 1, sizeof chunk - li->max_size + 1, handle);
		rd_sz += li->max_size - 1;
	}

	fclose(handle);

	return 0;
}

void last_index_dump_and_clean(struct last_index* li, FILE* steam){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		if (li->index[i] != NULL){
			last_entry_dump(li->index[i], steam);
			free(li->index[i]);
			li->index[i] = NULL;
		}
	}

	fprintf(stderr, "[+] %lu patterns dumped\n", li->nb_item);
}
