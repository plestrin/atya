#include <stdlib.h>
#include <stdint.h>

#include "abs_index.h"

static size_t si_get_size(union union_index* ui){
	return ui->asi.si->size;
}

static uint64_t si_compare_init(union union_index* ui, const uint8_t* data){
	ui->asi.hash = simple_index_hash_init(ui->asi.si, data);
	return simple_index_compare_hash(ui->asi.si, data, ui->asi.hash);
}

static uint64_t si_compare_next(union union_index* ui, const uint8_t* data){
	ui->asi.hash = simple_index_hash_next(ui->asi.si, ui->asi.hash, data);
	return simple_index_compare_hash(ui->asi.si, data + 1, ui->asi.hash);
}

static uint64_t si_compare_buffer(union union_index* ui, const uint8_t* buffer, size_t size){
	return simple_index_compare_buffer(ui->asi.si, buffer, size);
}

static uint64_t si_remove_nohit(union union_index* ui){
	return simple_index_remove_nohit(ui->asi.si);
}

void abs_index_init_simple(struct abs_index* ai, struct simple_index* si){
	ai->ui.asi.si = si;

	ai->index_get_size = si_get_size;

	ai->index_compare_init = si_compare_init;
	ai->index_compare_next = si_compare_next;

	ai->index_compare_buffer = si_compare_buffer;

	ai->index_remove_nohit = si_remove_nohit;
}
