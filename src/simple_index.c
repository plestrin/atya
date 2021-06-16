#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "simple_index.h"

#define SIMPLE_INDEX_ALLOC_STEP 16

static int simple_entry_insert(struct simple_entry** se_ptr, const uint8_t* ptr){
	struct simple_entry* se;
	struct simple_entry* new_se;

	se = *se_ptr;
	if (*se_ptr == NULL){
		if ((new_se = malloc(sizeof(struct simple_entry) + sizeof(struct simple_entry_item) * SIMPLE_INDEX_ALLOC_STEP)) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}

		*se_ptr = new_se;
		new_se->nb_alloc = SIMPLE_INDEX_ALLOC_STEP;
		new_se->nb_item = 0;
	}
	else if (se->nb_item == se->nb_alloc){
		if ((new_se = realloc(se, sizeof(struct simple_entry) + sizeof(struct simple_entry_item) * (SIMPLE_INDEX_ALLOC_STEP + se->nb_alloc))) == NULL){
			fprintf(stderr, "[-] in %s, unable to reallocate memory\n", __func__);
			return ENOMEM;
		}

		*se_ptr = new_se;
		new_se->nb_alloc += SIMPLE_INDEX_ALLOC_STEP;
	}
	se = *se_ptr;

	se->items[se->nb_item].status = STATUS_NONE;
	se->items[se->nb_item].ptr = ptr;
	se->nb_item ++;

	return 0;
}

static struct simple_entry_item* simple_entry_compare(struct simple_entry* se, const uint8_t* ptr, size_t size, uint8_t sel){
	uint32_t i;

	for (i = 0; i < se->nb_item; i++){
		if (!memcmp(se->items[i].ptr, ptr, size)){
			se->items[i].status |= sel;
			return se->items + i;
		}
	}

	return NULL;
}

static uint32_t simple_entry_remove(struct simple_entry* se, uint8_t sel){
	uint32_t i;
	uint32_t j;

	for (i = 0, j = se->nb_item; i < se->nb_item; i++){
		if ((se->items[i].status & STATUS_HIT) == (sel & STATUS_HIT)){
			for (j--; j > i; j--){
				if ((se->items[j].status & STATUS_HIT) != (sel & STATUS_HIT)){
					memcpy(se->items + i, se->items + j, sizeof(struct simple_entry_item));
					break;
				}
			}

			se->nb_item = j;
		}
		se->items[i].status = STATUS_NONE;
	}

	return j;
}

struct simple_index* simple_index_create(size_t size){
	struct simple_index* si;

	if ((si = malloc(sizeof(struct simple_index))) == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return NULL;
	}

	simple_index_init(si, size);

	return si;
}

int simple_index_insert_hash(struct simple_index* si, const uint8_t* ptr, uint16_t hash){
	int status = 0;

	if (si->index[hash] == NULL || simple_entry_compare(si->index[hash], ptr, si->size, 0) == NULL){
		if (!(status = simple_entry_insert(si->index + hash, ptr))){
			si->nb_item ++;
		}
	}

	return status;
}

struct simple_entry_item* simple_index_compare_hash(struct simple_index* si, const uint8_t* ptr, uint16_t hash, uint8_t sel){
	if (si->index[hash] != NULL){
		return simple_entry_compare(si->index[hash], ptr, si->size, sel);
	}

	return NULL;
}

uint64_t simple_index_compare_buffer(struct simple_index* si, const uint8_t* buffer, size_t size){
	size_t i;
	uint16_t hash;
	uint64_t cnt;

	if (size < si->size){
		return 0;
	}

	hash = simple_index_hash_init(si, buffer);
	cnt = (simple_index_compare_hash(si, buffer, hash, STATUS_HIT) != NULL);

	for (i = 1; i < size - si->size + 1; i++){
		hash = simple_index_hash_next(si, hash, buffer + i - 1);
		cnt += (simple_index_compare_hash(si, buffer + i, hash, STATUS_HIT) != NULL);
	}

	return cnt;
}

void simple_index_remove(struct simple_index* si, uint8_t sel){
	uint32_t i;
	uint32_t local_cnt;
	uint64_t cnt;

	for (i = 0, cnt = 0; i < 0x10000; i++){
		if (si->index[i] != NULL){
			local_cnt = simple_entry_remove(si->index[i], sel);
			if (!local_cnt){
				free(si->index[i]);
				si->index[i] = NULL;
			}
			else {
				cnt += local_cnt;
			}
		}
	}
	si->nb_item = cnt;
}

void simple_index_clean(struct simple_index* si){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		free(si->index[i]);
		si->index[i] = NULL;
	}
	si->nb_item = 0;
}
