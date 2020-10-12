#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "last_index.h"
#include "hash.h"

static inline uint8_t* last_entry_get_data(struct last_entry* le){
	return (uint8_t*)(le + 1);
}

static struct last_entry* last_entry_push(struct last_entry* le, const uint8_t* data, size_t size){
	size_t new_alloc_size;
	struct last_entry* new_le;
	int status;
	size_t alloc_size = 0;
	size_t used_size = 0;

	if (le != NULL){
		alloc_size = le->alloc_size;
		used_size = le->used_size;
	}

	if (sizeof(size_t) > alloc_size - used_size || size > alloc_size - used_size - sizeof(size_t)){
		new_alloc_size = sizeof(struct last_entry) + size + sizeof(size_t) + used_size;
		if (new_alloc_size & 0xfff){
			new_alloc_size = ((new_alloc_size >> 12) + 1) << 12;
		}
		else {
			new_alloc_size = (new_alloc_size >> 12) << 12;
		}

		if (used_size > new_alloc_size || sizeof(size_t) > new_alloc_size - used_size || size > new_alloc_size - used_size - sizeof(size_t) || sizeof(struct last_entry) > new_alloc_size - used_size - sizeof(size_t) - size){
			fprintf(stderr, "[-] in %s, integer overflow, size argument is too big\n", __func__);
			return NULL;
		}

		if ((new_le = realloc(le, new_alloc_size)) == NULL){
			status = errno;
			fprintf(stderr, "[-] in %s, unable to realloc memory\n", __func__);
			return NULL;
		}

		le = new_le;
		le->alloc_size = new_alloc_size - sizeof(struct last_entry);
		le->used_size = used_size;
	}

	*(size_t*)(last_entry_get_data(le) + le->used_size) = size;
	le->used_size += sizeof(size_t);
	memcpy(last_entry_get_data(le) + le->used_size, data, size);
	le->used_size += size;

	return le;
}

static uint32_t last_entry_exclude(struct last_entry* le, const uint8_t* data, size_t size){
	size_t offset;
	size_t item_size;

	for (offset = 0; offset < le->used_size; offset += sizeof(size_t) + item_size){
		item_size = *(size_t*)(last_entry_get_data(le) + offset);
		if (item_size <= size && !memcmp(last_entry_get_data(le) + offset + sizeof(size_t), data, item_size)){
			le->used_size -= sizeof(size_t) + item_size;
			memmove(last_entry_get_data(le) + offset, last_entry_get_data(le) + offset + sizeof(size_t) + item_size, le->used_size - offset);
			return 1;
		}
	}

	return 0;
}

static void last_entry_dump(struct last_entry* le, FILE* stream){
	size_t offset;
	size_t item_size;

	for (offset = 0; offset < le->used_size; offset += item_size + sizeof(size_t)){
		item_size = *(size_t*)(last_entry_get_data(le) + offset);
		fwrite(last_entry_get_data(le) + offset, item_size + sizeof(size_t), 1, stream);
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

void last_index_exclude_buffer(struct last_index* li, const uint8_t* buffer, size_t size, size_t overlap){
	uint16_t hash;
	size_t offset;

	for (offset = 0; offset + overlap <= size; offset ++){
		hash = hash_init(buffer + offset, li->min_size);
		if (li->index[hash] != NULL){
			li->nb_item -= last_entry_exclude(li->index[hash], buffer + offset, size - offset);
			if (!li->index[hash]->used_size){
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

	rd_sz = fread(chunk, 1, sizeof chunk, handle);
	for ( ; ; ){
		if (rd_sz < sizeof chunk){
			last_index_exclude_buffer(li, chunk, rd_sz, li->min_size);
			break;
		}

		last_index_exclude_buffer(li, chunk, rd_sz, li->max_size);
		memmove(chunk, chunk + rd_sz - li->max_size + 1, li->max_size - 1);
		rd_sz = fread(chunk + li->max_size - 1, 1, (sizeof chunk) - li->max_size + 1, handle);
		if (!rd_sz){
			break;
		}
		rd_sz += li->max_size - 1;
	}

	fclose(handle);

	return 0;
}

void last_index_dump(struct last_index* li, FILE* steam){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		if (li->index[i] != NULL){
			last_entry_dump(li->index[i], steam);
		}
	}

	fprintf(stderr, "[+] in %s, %lu patterns dumped\n", __func__, li->nb_item);
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

	fprintf(stderr, "[+] in %s, %lu patterns dumped\n", __func__, li->nb_item);
}

void last_index_clean(struct last_index* li){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		free(li->index[i]);
		li->index[i] = NULL;
	}
}
