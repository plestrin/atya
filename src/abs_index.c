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

static uint64_t si_remove_hit(union union_index* ui){
	return simple_index_remove_hit(ui->asi.si);
}

static uint64_t si_remove_nohit(union union_index* ui){
	return simple_index_remove_nohit(ui->asi.si);
}

static void si_clean(union union_index* ui){
	simple_index_clean(ui->asi.si);
}

void abs_index_init_simple(struct abs_index* ai, struct simple_index* si){
	ai->ui.asi.si = si;

	ai->index_get_size = si_get_size;

	ai->index_compare_init = si_compare_init;
	ai->index_compare_next = si_compare_next;

	ai->index_compare_buffer = si_compare_buffer;

	ai->index_remove_hit = si_remove_hit;
	ai->index_remove_nohit = si_remove_nohit;

	ai->index_clean = si_clean;
}

static size_t fi_get_size(union union_index* ui){
	return ui->afi.fi->size;
}

static uint64_t fi_compare_init(union union_index* ui, const uint8_t* data){
	return fast_index_compare(ui->afi.fi, data);
}

static uint64_t fi_compare_next(union union_index* ui, const uint8_t* data){
	return fast_index_compare(ui->afi.fi, data + 1);
}

static uint64_t fi_compare_buffer(union union_index* ui, const uint8_t* buffer, size_t size){
	return fast_index_compare_buffer(ui->afi.fi, buffer, size);
}

static uint64_t fi_remove_hit(union union_index* ui){
	return fast_index_remove_hit(ui->afi.fi);
}

static uint64_t fi_remove_nohit(union union_index* ui){
	return fast_index_remove_nohit(ui->afi.fi);
}

static void fi_clean(union union_index* ui){
	fast_index_clean(ui->afi.fi);
}

void abs_index_init_fast(struct abs_index* ai, struct fast_index* fi){
	ai->ui.afi.fi = fi;

	ai->index_get_size = fi_get_size;

	ai->index_compare_init = fi_compare_init;
	ai->index_compare_next = fi_compare_next;

	ai->index_compare_buffer = fi_compare_buffer;

	ai->index_remove_hit = fi_remove_hit;
	ai->index_remove_nohit = fi_remove_nohit;

	ai->index_clean = fi_clean;
}
