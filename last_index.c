#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "last_index.h"
#include "hash.h"

static struct last_entry* last_entry_create(void){
	struct last_entry* le;

	if ((le = calloc(1, sizeof(struct last_entry))) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
	}

	return le;
}

static int last_entry_push(struct last_entry* le, const uint8_t* data, size_t size){
	size_t new_alloc_size;
	void* new_ptr;
	int status;

	if (sizeof(size_t) > le->alloc_size - le->used_size || size > le->alloc_size - le->used_size - sizeof(size_t)){
		new_alloc_size = size + sizeof(size_t) + le->used_size;
		if (new_alloc_size & 0xfff){
			new_alloc_size = ((new_alloc_size >> 12) + 1) << 12;
		}
		else {
			new_alloc_size = (new_alloc_size >> 12) << 12;
		}

		if (sizeof(size_t) > new_alloc_size - le->used_size || size > new_alloc_size - le->used_size - sizeof(size_t)){
			fprintf(stderr, "[-] in %s, integer overflow, size argument is too big\n", __func__);
			return EINVAL;
		}

		if ((new_ptr = realloc(le->ptr, new_alloc_size)) == NULL){
			status = errno;
			fprintf(stderr, "[-] in %s, unable to realloc memory\n", __func__);
			return status;
		}

		le->ptr = new_ptr;
		le->alloc_size = new_alloc_size;
	}

	*(size_t*)(le->ptr + le->used_size) = size;
	le->used_size += sizeof(size_t);
	memcpy(le->ptr + le->used_size, data, size);
	le->used_size += size;

	return 0;
}

static uint64_t last_entry_exclude(struct last_entry* le, const uint8_t* data, size_t size){
	size_t offset_read;
	size_t offset_write;
	size_t item_size;
	uint64_t nb_item;

	for (offset_read = 0, offset_write = 0, nb_item = 0; offset_read < le->used_size; offset_read += sizeof(size_t) + item_size){
		item_size = *(size_t*)(le->ptr + offset_read);
		if (item_size <= size && !memcmp(le->ptr + offset_read + sizeof(size_t), data, item_size)){
			nb_item ++;
			continue;
		}
		if (offset_write != offset_read){
			memmove(le->ptr + offset_write, le->ptr + offset_read, sizeof(size_t) + item_size);
		}
		offset_write += sizeof(size_t) + item_size;
	}

	le->used_size = offset_write;

	return nb_item;
}

static void last_entry_dump(struct last_entry* le, FILE* stream){
	size_t offset;
	size_t item_size;

	for (offset = 0; offset < le->used_size; offset += item_size + sizeof(size_t)){
		item_size = *(size_t*)(le->ptr + offset);
		fwrite(le->ptr + offset, item_size + sizeof(size_t), 1, stream);
	}
}

static void last_entry_delete(struct last_entry* le){
	free(le->ptr);
	free(le);
}

void last_index_init(struct last_index* li, size_t min_size){
	li->min_size = min_size;
	li->max_size = 0;
	memset(li->index, 0, sizeof li->index);
}

int last_index_push(struct last_index* li, const uint8_t* data, size_t size){
	uint16_t hash;
	int status;

	if (size < li->min_size){
		fprintf(stderr, "[-] in %s, size argument is too small\n", __func__);
		return EINVAL;
	}

	hash = hash_init(data, li->min_size);
	if (li->index[hash] == NULL){
		if ((li->index[hash] = last_entry_create()) == NULL){
			fprintf(stderr, "[-] in %s, unable to create last_entry\n", __func__);
			return ENOMEM;
		}
	}

	if (!(status = last_entry_push(li->index[hash], data, size))){
		li->nb_item ++;
		if (size > li->max_size){
			li->max_size = size;
		}
	}

	return status;
}

void last_index_exclude_buffer(struct last_index* li, const uint8_t* buffer, size_t size, size_t overlap){
	uint16_t hash;
	size_t offset;

	for (offset = 0; offset + overlap <= size; offset ++){
		hash = hash_init(buffer + offset, li->min_size);
		if (li->index[hash] != NULL){
			li->nb_item -= last_entry_exclude(li->index[hash], buffer + offset, size - offset);
			if (!li->index[hash]->used_size){
				last_entry_delete(li->index[hash]);
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

void last_index_clean(struct last_index* li){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		if (li->index[i] != NULL){
			last_entry_delete(li->index[i]);
			li->index[i] = NULL;
		}
	}
}
