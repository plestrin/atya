#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "fast_index.h"

static int index_insert(void** root, const uint8_t* value, size_t index_size){
	uint8_t idx;
	uint8_t* leaf;

	idx = value[0];
	if (index_size > 2){
		if (root[idx] == NULL){
			root[idx] = calloc(0x100, sizeof(void*));
		}
	}
	else if (index_size > 1){
		if (root[idx] == NULL){
			root[idx] = calloc(0x40, 1);
		}
	}
	else {
		leaf = (uint8_t*)root;
		leaf[idx >> 2] |= 1 << (2 * (idx & 0x3));
		return 0;
	}

	if (root[idx] == NULL){
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		return ENOMEM;
	}

	return index_insert(root[idx], value + 1, index_size - 1);
}

int fast_index_insert(struct fast_index* fi, const uint8_t* value){
	uint16_t idx;

	idx = *(uint16_t*)value;
	if (fi->root[idx] == NULL){
		if ((fi->root[idx] = calloc(0x100, sizeof(void*))) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}
	}

	return index_insert(fi->root[idx], value + 2, fi->size - 2);
}

int fast_index_insert_buffer(struct fast_index* fi, const uint8_t* buffer, size_t size){
	size_t i;
	int status = 0;

	if (size < fi->size - 1){
		return 0;
	}

	for (i = 0; i < size - fi->size + 1; i++){
		if ((status = fast_index_insert(fi, buffer + i))){
			break;
		}
	}

	return status;
}

static uint64_t index_compare(void** root, const uint8_t* value, size_t index_size){
	uint8_t idx;
	uint8_t* leaf;
	uint64_t res = 0;

	idx = value[0];
	if (index_size > 1){
		if (root[idx] != NULL){
			res = index_compare(root[idx], value + 1, index_size - 1);
		}
	}
	else {
		leaf = (uint8_t*)root;
		res = (leaf[idx >> 2] >> (2 * (idx & 0x3))) & 0x1;
		leaf[idx >> 2] |= 1 << (2 * (idx & 0x3) + 1);
	}

	return res;
}

uint64_t fast_index_compare(struct fast_index* fi, const uint8_t* value){
	uint16_t idx;

	idx = *(uint16_t*)value;
	if (fi->root[idx] != NULL){
		return index_compare(fi->root[idx], value + 2, fi->size - 2);
	}

	return 0;
}

uint64_t fast_index_compare_buffer(struct fast_index* fi, const uint8_t* buffer, size_t size){
	size_t i;
	uint64_t cnt;

	if (size < fi->size){
		return 0;
	}

	for (i = 0, cnt = 0; i < size - fi->size + 1; i++){
		cnt += fast_index_compare(fi, buffer + i);
	}

	return cnt;
}

static int index_get_first(void** root, size_t index_size, uint8_t* value, size_t mask_size){
	uint32_t idx;
	uint8_t* leaf;

	idx = value[0];
	if (!mask_size){
		if (index_size > 1){
			for ( ; idx < 0x100; idx ++){
				if (root[idx] == NULL){
					continue;
				}

				if (index_get_first(root[idx], index_size - 1, value + 1, 0)){
					value[0] = idx;
					return 1;
				}

				value[1] = 0;
			}
		}
		else {
			leaf = (uint8_t*)root;
			for ( ; idx < 0x100; idx ++){
				if (leaf[idx >> 2] & (1 << (2 * (idx & 0x3)))){
					value[0] = idx;
					return 1;
				}
			}
		}
	}
	else {
		if (index_size > 1){
			if (root[idx] != NULL){
				return index_get_first(root[idx], index_size - 1, value + 1, mask_size - 1);
			}
		}
		else {
			leaf = (uint8_t*)root;
			return leaf[idx >> 2] & (1 << (2 * (idx & 0x3)));
		}
	}

	return 0;
}

static int fast_index_get(struct fast_index* fi, uint8_t* value, size_t mask_size){
	uint32_t idx;

	idx = *(uint16_t*)value;
	if (!mask_size || mask_size == 1){
		for ( ; idx < 0x10000; idx ++){
			if (fi->root[idx] == NULL){
				continue;
			}

			if (index_get_first(fi->root[idx], fi->size - 2, value + 2, 0)){
				*(uint16_t*)value = idx;
				return 1;
			}

			value[2] = 0;
		}

	}
	else if (fi->root[idx] != NULL){
		return index_get_first(fi->root[idx], fi->size - 2, value + 2, mask_size - 2);
	}

	return 0;
}

int fast_index_get_first(struct fast_index* fi, uint8_t* value, size_t mask_size){
	size_t i;

	if (mask_size == 1){
		mask_size = 0;
	}
	for (i = fi->size; i > mask_size; i --){
		value[i - 1] = 0;
	}

	return fast_index_get(fi, value, mask_size);
}

int fast_index_get_next(struct fast_index* fi, uint8_t* value, size_t mask_size){
	size_t i;

	for (i = fi->size; i > mask_size && i > 2; i --){
		if (value[i - 1] != 0xff){
			value[i - 1] += 1;
			return fast_index_get(fi, value, mask_size);
		}

		value[i - 1] = 0;
	}
	if (mask_size < 2){
		if (*(uint16_t*)value != 0xffff){
			*(uint16_t*)value += 1;
			return fast_index_get(fi, value, 0);
		}
	}

	return 0;
}

static void index_clean(void** root, size_t index_size){
	uint32_t i;

	for (i = 0; i < 0x100; i++){
		if (root[i] != NULL){
			if (index_size > 2){
				index_clean(root[i], index_size - 1);
			}
			free(root[i]);
			root[i] = NULL;
		}
	}
}

void fast_index_clean(struct fast_index* fi){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		if (fi->root[i] != NULL){
			index_clean(fi->root[i], fi->size - 2);
			free(fi->root[i]);
			fi->root[i] = NULL;
		}
	}
}

void fast_index_delete(struct fast_index* fi){
	fast_index_clean(fi);
	free(fi);
}

static uint64_t index_count(void** root, size_t index_size){
	uint32_t i;
	uint64_t cnt = 0;

	if (index_size > 1){
		for (i = 0; i < 0x100; i++){
			if (root[i] != NULL){
				cnt += index_count(root[i], index_size - 1);
			}
		}
	}
	else {
		uint64_t* leaf = (uint64_t*)root;

		for (i = 0; i < 8; i++){
			cnt += __builtin_popcountll(leaf[i]);
		}
	}

	return cnt;
}

uint64_t fast_index_count(struct fast_index* fi){
	uint32_t i;
	uint64_t cnt;

	for (i = 0, cnt = 0; i < 0x10000; i++){
		if (fi->root[i] != NULL){
			cnt += index_count(fi->root[i], fi->size - 2);
		}
	}

	return cnt;
}

static uint64_t index_remove(void** root, size_t index_size, uint64_t sel){
	uint32_t i;
	uint64_t cnt;

	if (index_size > 1){
		uint64_t local_cnt;

		for (i = 0, cnt = 0; i < 0x100; i++){
			if (root[i] != NULL){
				local_cnt = index_remove(root[i], index_size - 1, sel);
				if (!local_cnt){
					free(root[i]);
					root[i] = NULL;
				}
				else {
					cnt += local_cnt;
				}
			}
		}
	}
	else {
		uint64_t* leaf = (uint64_t*)root;

		for (i = 0, cnt = 0; i < 8; i++){
			leaf[i] &= ((leaf[i] >> 1) & 0x5555555555555555) ^ sel;
			cnt += __builtin_popcountll(leaf[i]);
		}
	}

	return cnt;
}

uint64_t fast_index_remove(struct fast_index* fi, uint64_t sel){
	uint32_t i;
	uint64_t local_cnt;
	uint64_t cnt;

	for (i = 0, cnt = 0; i < 0x10000; i++){
		if (fi->root[i] != NULL){
			local_cnt = index_remove(fi->root[i], fi->size - 2, sel);
			if (!local_cnt){
				free(fi->root[i]);
				fi->root[i] = NULL;
			}
			else {
				cnt += local_cnt;
			}
		}
	}

	return cnt;
}
