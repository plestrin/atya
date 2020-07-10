#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>

#include "index.h"

static int index_do_insert(void** root, const uint8_t* value, size_t index_size){
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

	return index_do_insert(root[idx], value + 1, index_size - 1);
}

int index_root_do_insert(struct index_root* ir, const uint8_t* value){
	uint16_t idx;

	idx = *(uint16_t*)value;
	if (ir->root[idx] == NULL){
		if ((ir->root[idx] = calloc(0x100, sizeof(void*))) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			return ENOMEM;
		}
	}

	return index_do_insert(ir->root[idx], value + 2, ir->size - 2);
}

int index_root_do_insert_buffer(struct index_root* ir, const uint8_t* buffer, size_t size){
	size_t i;
	int status = 0;

	if (size < ir->size - 1){
		return 0;
	}

	for (i = 0; i < size - ir->size + 1; i++){
		if ((status = index_root_do_insert(ir, buffer + i))){
			break;
		}
	}

	return status;
}

static void index_do_compare(void** root, const uint8_t* value, size_t index_size){
	uint8_t idx;
	uint8_t* leaf;

	idx = value[0];
	if (index_size > 1){
		if (root[idx] != NULL){
			index_do_compare(root[idx], value + 1, index_size - 1);
		}
	}
	else {
		leaf = (uint8_t*)root;
		leaf[idx >> 2] |= 1 << (2 * (idx & 0x3) + 1);
	}
}

void index_root_do_compare(struct index_root* ir, const uint8_t* value){
	uint16_t idx;

	idx = *(uint16_t*)value;
	if (ir->root[idx] != NULL){
		index_do_compare(ir->root[idx], value + 2, ir->size - 2);
	}
}

void index_root_do_compare_buffer(struct index_root* ir, const uint8_t* buffer, size_t size){
	size_t i;

	if (size < ir->size){
		return;
	}

	for (i = 0; i < size - ir->size + 1; i++){
		index_root_do_compare(ir, buffer + i);
	}
}

static int index_do_get_first(void** root, size_t index_size, uint8_t* value, size_t mask_size){
	uint32_t idx;
	uint8_t* leaf;

	idx = value[0];
	if (!mask_size){
		if (index_size > 1){
			for ( ; idx < 0x100; idx ++){
				if (root[idx] == NULL){
					continue;
				}

				if (index_do_get_first(root[idx], index_size - 1, value + 1, 0)){
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
				return index_do_get_first(root[idx], index_size - 1, value + 1, mask_size - 1);
			}
		}
		else {
			leaf = (uint8_t*)root;
			return leaf[idx >> 2] & (1 << (2 * (idx & 0x3)));
		}
	}

	return 0;
}

static int index_root_do_get(struct index_root* ir, uint8_t* value, size_t mask_size){
	uint32_t idx;

	idx = *(uint16_t*)value;
	if (!mask_size || mask_size == 1){
		for ( ; idx < 0x10000; idx ++){
			if (ir->root[idx] == NULL){
				continue;
			}

			if (index_do_get_first(ir->root[idx], ir->size - 2, value + 2, 0)){
				*(uint16_t*)value = idx;
				return 1;
			}

			value[2] = 0;
		}

	}
	else if (ir->root[idx] != NULL){
		return index_do_get_first(ir->root[idx], ir->size - 2, value + 2, mask_size - 2);
	}

	return 0;
}

int index_root_do_get_first(struct index_root* ir, uint8_t* value, size_t mask_size){
	size_t i;

	if (mask_size == 1){
		mask_size = 0;
	}
	for (i = ir->size; i > mask_size; i --){
		value[i - 1] = 0;
	}

	return index_root_do_get(ir, value, mask_size);
}

int index_root_do_get_next(struct index_root* ir, uint8_t* value, size_t mask_size){
	size_t i;

	for (i = ir->size; i > mask_size && i > 2; i --){
		if (value[i - 1] != 0xff){
			value[i - 1] += 1;
			return index_root_do_get(ir, value, mask_size);
		}

		value[i - 1] = 0;
	}
	if (mask_size < 2){
		if (*(uint16_t*)value != 0xffff){
			*(uint16_t*)value += 1;
			return index_root_do_get(ir, value, 0);
		}
	}

	return 0;
}

static int index_do_test(void** root, const uint8_t* value, size_t index_size){
	uint8_t idx = value[0];

	if (index_size > 1){
		if (root[idx] != NULL){
			return index_do_test(root[idx], value + 1, index_size - 1);
		}
	}

	return *((uint8_t*)root + (idx >> 2)) & (1 << (2 * (idx & 0x3)));
}

int index_root_do_test(struct index_root* ir, const uint8_t* value){
	uint16_t idx;

	idx = *(uint16_t*)value;
	if (ir->root[idx] != NULL){
		return index_do_test(ir->root[idx], value + 2, ir->size - 2);
	}

	return 0;
}

static void index_do_clean(void** root, size_t index_size){
	uint32_t i;

	for (i = 0; i < 0x100; i++){
		if (root[i] != NULL){
			if (index_size > 2){
				index_do_clean(root[i], index_size - 1);
			}
			free(root[i]);
			root[i] = NULL;
		}
	}
}

void index_root_do_clean(struct index_root* ir){
	uint32_t i;

	for (i = 0; i < 0x10000; i++){
		if (ir->root[i] != NULL){
			index_do_clean(ir->root[i], ir->size - 2);
			free(ir->root[i]);
			ir->root[i] = NULL;
		}
	}
}

void index_root_do_delete(struct index_root* ir){
	index_root_do_clean(ir);
	free(ir);
}

static uint64_t index_do_count(void** root, size_t index_size){
	uint32_t i;
	uint64_t cnt = 0;

	if (index_size > 1){
		for (i = 0; i < 0x100; i++){
			if (root[i] != NULL){
				cnt += index_do_count(root[i], index_size - 1);
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

uint64_t index_root_do_count(struct index_root* ir){
	uint32_t i;
	uint64_t cnt;

	for (i = 0, cnt = 0; i < 0x10000; i++){
		if (ir->root[i] != NULL){
			cnt += index_do_count(ir->root[i], ir->size - 2);
		}
	}

	return cnt;
}

static int index_do_intersect(void** root, size_t index_size){
	uint32_t i;
	uint64_t nb_valid;

	if (index_size > 1){
		for (i = 0, nb_valid = 0; i < 0x100; i++){
			if (root[i] != NULL){
				if (index_do_intersect(root[i], index_size - 1)){
					free(root[i]);
					root[i] = NULL;
				}
				else {
					nb_valid ++;
				}
			}
		}
	}
	else {
		uint64_t* leaf = (uint64_t*)root;

		for (i = 0, nb_valid = 0; i < 8; i++){
			leaf[i] &= (leaf[i] >> 1) & 0x5555555555555555;
			nb_valid += __builtin_popcountll(leaf[i]);
		}
	}

	return !nb_valid;
}

int index_root_do_intersect(struct index_root* ir){
	uint32_t i;
	uint32_t nb_valid;

	for (i = 0, nb_valid = 0; i < 0x10000; i++){
		if (ir->root[i] != NULL){
			if (index_do_intersect(ir->root[i], ir->size - 2)){
				free(ir->root[i]);
				ir->root[i] = NULL;
			}
			else {
				nb_valid ++;
			}
		}
	}

	return !nb_valid;
}

static int index_do_exclude(void** root, size_t index_size){
	uint32_t i;
	uint64_t nb_valid;

	if (index_size > 1){
		for (i = 0, nb_valid = 0; i < 0x100; i++){
			if (root[i] != NULL){
				if (index_do_exclude(root[i], index_size - 1)){
					free(root[i]);
					root[i] = NULL;
				}
				else {
					nb_valid ++;
				}
			}
		}
	}
	else {
		uint64_t* leaf = (uint64_t*)root;

		for (i = 0, nb_valid = 0; i < 8; i++){
			leaf[i] = (leaf[i] & (~(leaf[i] >> 1))) & 0x5555555555555555;
			nb_valid += __builtin_popcountll(leaf[i]);
		}
	}

	return !nb_valid;
}

int index_root_do_exclude(struct index_root* ir){
	uint32_t i;
	uint32_t nb_valid;

	for (i = 0, nb_valid = 0; i < 0x10000; i++){
		if (ir->root[i] != NULL){
			if (index_do_exclude(ir->root[i], ir->size - 2)){
				free(ir->root[i]);
				ir->root[i] = NULL;
			}
			else {
				nb_valid ++;
			}
		}
	}

	return !nb_valid;
}
