#ifndef STREE_H
#define STREE_H

#include <stdlib.h>
#include <stdint.h>

struct snode {
	uint16_t nb_child;
	uint16_t parentlo;
	uint32_t parenthi;
	uint32_t edgehi;
	size_t size;
	const uint8_t* ptr;
	size_t hit_size;
	uint16_t edgelo[256];
};

struct sroot {
	uint16_t edgelo[256];
};

struct indexlo {
	uint32_t nb_alloc;
	uint32_t nb_reserved;
	uint32_t nb_used;
	struct snode buffer[];
};

struct indexhi {
	uint32_t nb_alloc;
	uint32_t current;
	struct indexlo* buffer[];
};

struct stree {
	struct sroot root;
	struct indexhi* index;
};

int stree_init(struct stree* tree);
int stree_insert(struct stree* tree, const uint8_t* ptr, size_t size);
int stree_insert_all(struct stree* tree, const uint8_t* ptr, size_t size, size_t min_size);
int stree_intersect(struct stree* tree, const uint8_t* ptr, size_t size);
int stree_intersect_all(struct stree* tree, const uint8_t* ptr, size_t size, size_t min_size);
void stree_prune(struct stree* tree);
void stree_clean(struct stree* tree);

struct stree_iterator {
	uint8_t* ptr;
	size_t alloc_size;
	size_t offset;
	uint32_t edgehi;
	uint16_t edgelo;
};

int stree_iterator_init(struct stree_iterator* sti, struct stree* tree);
int stree_iterator_next(struct stree_iterator* sti, struct stree* tree);
void stree_iterator_clean(struct stree_iterator* sti);

#endif
