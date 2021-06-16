#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include "reverse_index.h"
#include "hash.h"

int reverse_index_init(struct reverse_index* ri, size_t min_size){
	if ((ri->gsk = gory_sewer_create(0x20000)) == NULL){
		return ENOMEM;
	}

	ri->min_size = min_size;

	memset(ri->index, 0, sizeof ri->index);

	return 0;
}

struct reverse_index* reverse_index_create(size_t min_size){
	struct reverse_index* ri;

	if ((ri = malloc(sizeof(struct reverse_index))) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return NULL;
	}

	if (reverse_index_init(ri, min_size)){
		fprintf(stderr, "[-] in %s, unable to initialize reverse index\n", __func__);
		free(ri);
		ri = NULL;
	}

	return ri;
}

#define REVERSE_INDEX_ALLOC_STEP 32

int reverse_index_append(struct reverse_index* ri, const uint8_t* ptr, size_t size){
	uint16_t hash;
	uint32_t j;

	if (size < ri->min_size){
		fprintf(stderr, "[-] in %s, too small\n", __func__);
		return EINVAL;
	}

	hash = hash_init(ptr + size - ri->min_size, ri->min_size);
	if (ri->index[hash] == NULL){
		struct reverse_entry* re;

		if ((re = malloc(sizeof(struct reverse_entry) + sizeof(struct reverse_entry_item) * REVERSE_INDEX_ALLOC_STEP)) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}

		re->nb_alloc = REVERSE_INDEX_ALLOC_STEP;
		re->nb_item = 0;

		ri->index[hash] = re;
	}

	for (j = 0; j < ri->index[hash]->nb_item; j++){
		if (size < ri->index[hash]->items[j].size){
			if (!memcmp(ptr, ri->index[hash]->items[j].ptr + ri->index[hash]->items[j].size - size, size)){
				return 0;
			}
		}
		if (size > ri->index[hash]->items[j].size){
			if (!memcmp(ptr + size - ri->index[hash]->items[j].size, ri->index[hash]->items[j].ptr, ri->index[hash]->items[j].size)){
				if ((ptr = gory_sewer_push(ri->gsk, ptr, size)) == NULL){
					fprintf(stderr, "[-] in %s, unable to push data to gory sewer\n", __func__);
					return ENOMEM;
				}

				ri->index[hash]->items[j].ptr = ptr;
				ri->index[hash]->items[j].size = size;
				return 0;
			}
		}
	}

	if (ri->index[hash]->nb_item == ri->index[hash]->nb_alloc){
		struct reverse_entry* re;

		if ((re = realloc(ri->index[hash], sizeof(struct reverse_entry) + sizeof(struct reverse_entry_item) * (REVERSE_INDEX_ALLOC_STEP + ri->index[hash]->nb_alloc))) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}

		re->nb_alloc += REVERSE_INDEX_ALLOC_STEP;

		ri->index[hash] = re;
	}

	if ((ptr = gory_sewer_push(ri->gsk, ptr, size)) == NULL){
		fprintf(stderr, "[-] in %s, unable to push data to gory sewer\n", __func__);
		return ENOMEM;
	}

	ri->index[hash]->items[ri->index[hash]->nb_item].ptr = ptr;
	ri->index[hash]->items[ri->index[hash]->nb_item].size = size;

	ri->index[hash]->nb_item ++;

	return 0;
}

void reverse_index_dump(struct reverse_index* ri, FILE* stream){
	uint32_t i;
	uint32_t j;
	uint64_t size;

	for (i = 0; i < 0x10000; i++){
		if (ri->index[i] != NULL){
			for (j = 0; j < ri->index[i]->nb_item; j++){
				size = ri->index[i]->items[j].size;
				fwrite(&size, sizeof size, 1, stream);
				fwrite(ri->index[i]->items[j].ptr, ri->index[i]->items[j].size, 1, stream);
			}
		}
	}
}

void reverse_index_clean(struct reverse_index* ri){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		free(ri->index[i]);
	}

	gory_sewer_delete(ri->gsk);
}
