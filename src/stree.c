#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "stree.h"

static size_t get_common_size(const uint8_t* ptr1, const uint8_t* ptr2, size_t ptr1_sz, size_t ptr2_sz){
	size_t i;

	for (i = 1; i < ptr1_sz && i < ptr2_sz; i++){
		if (ptr1[i] != ptr2[i]){
			break;
		}
	}

	return i;
}

#define INVALID_EDGE_LO 0xffff
#define INVALID_EDGE_HI 0xffffffff

#define INDEX_LO_ALLOC_STEP 128

static struct indexlo* indexlo_create(void){
	struct indexlo* index;

	if ((index = malloc(sizeof(struct indexlo) + sizeof(struct snode) * INDEX_LO_ALLOC_STEP)) == NULL){
		return NULL;
	}

	index->nb_alloc = INDEX_LO_ALLOC_STEP;
	index->nb_reserved = 0;
	index->nb_used = 0;

	return index;
}

static struct indexlo* indexlo_realloc(struct indexlo* index){
	struct indexlo* index_new;

	if ((index_new = realloc(index, sizeof(struct indexlo) + sizeof(struct snode) * (INDEX_LO_ALLOC_STEP + index->nb_alloc))) == NULL){
		return NULL;
	}

	index_new->nb_alloc += INDEX_LO_ALLOC_STEP;

	return index_new;
}

static struct indexlo* index_dealloc(struct indexlo* index){
	struct indexlo* index_new;

	if (!index->nb_used){
		return NULL;
	}

	if (index->nb_alloc - index->nb_used > INDEX_LO_ALLOC_STEP){
		return index;
	}

	if ((index_new = realloc(index, sizeof(struct indexlo) + sizeof(struct snode) * index->nb_used)) == NULL){
		return index;
	}

	index_new->nb_alloc = index_new->nb_used;

	return index_new;
}

#define INDEX_HI_ALLOC_STEP 1024

static struct indexhi* indexhi_create(void){
	struct indexhi* index;

	if ((index = malloc(sizeof(struct indexhi) + sizeof(struct indexlo*) * INDEX_HI_ALLOC_STEP)) == NULL){
		return NULL;
	}

	index->nb_alloc = INDEX_HI_ALLOC_STEP;
	index->current = 0;

	memset(index->buffer, 0, sizeof(struct indexlo*) * INDEX_HI_ALLOC_STEP);

	return index;
}

static struct indexhi* indexhi_realloc(struct indexhi* index){
	struct indexhi* index_new;

	if ((index_new = realloc(index, sizeof(struct indexhi) + sizeof(struct indexlo*) * (INDEX_HI_ALLOC_STEP + index->nb_alloc))) == NULL){
		return NULL;
	}

	memset(index_new->buffer + index_new->nb_alloc, 0, sizeof(struct indexlo*) * INDEX_HI_ALLOC_STEP);

	index_new->nb_alloc += INDEX_HI_ALLOC_STEP;

	return index_new;
}

static void indexhi_clean(struct indexhi* index){
	uint32_t i;

	for (i = 0; i <= index->current; i++){
		free(index->buffer[i]);
	}
}


static uint32_t stree_reserve_edges(struct stree* tree){
	if (tree->index == NULL){
		if ((tree->index = indexhi_create()) == NULL){
			return INVALID_EDGE_HI;
		}
	}

	if (tree->index->buffer[tree->index->current] == NULL){
		if ((tree->index->buffer[tree->index->current] = indexlo_create()) == NULL){
			return INVALID_EDGE_HI;
		}
	}

	if (tree->index->buffer[tree->index->current]->nb_reserved < 0xff00){
		tree->index->buffer[tree->index->current]->nb_reserved += 0x100;
		return tree->index->current;
	}

	if (tree->index->current + 1 == tree->index->nb_alloc){
		struct indexhi* new_index;

		if ((new_index = indexhi_realloc(tree->index)) == NULL){
			return INVALID_EDGE_HI;
		}

		tree->index = new_index;
	}

	tree->index->current += 1;

	if ((tree->index->buffer[tree->index->current] = indexlo_create()) == NULL){
		return INVALID_EDGE_HI;
	}

	tree->index->buffer[tree->index->current]->nb_reserved = 0x100;

	return tree->index->current;
}

static uint16_t stree_alloc_edge(struct stree* tree, uint32_t edgehi, int reserve){
	uint16_t edgelo;

	if (edgehi >= tree->index->nb_alloc || tree->index->buffer[edgehi] == NULL){
		return INVALID_EDGE_LO;
	}

	if (tree->index->buffer[edgehi]->nb_used == tree->index->buffer[edgehi]->nb_alloc){
		struct indexlo* index_new;

		if ((index_new = indexlo_realloc(tree->index->buffer[edgehi])) == NULL){
			return INVALID_EDGE_LO;
		}

		tree->index->buffer[edgehi] = index_new;
	}

	edgelo = tree->index->buffer[edgehi]->nb_used;

	if (reserve){
		if ((tree->index->buffer[edgehi]->buffer[edgelo].edgehi = stree_reserve_edges(tree)) == INVALID_EDGE_HI){
			return INVALID_EDGE_LO;
		}
	}

	tree->index->buffer[edgehi]->nb_used ++;

	return edgelo;
}

int stree_init(struct stree* tree){
	memset(tree->root.edgelo, 0xff, sizeof tree->root.edgelo);
	tree->index = NULL;

	if (stree_reserve_edges(tree) == INVALID_EDGE_HI){
		return -1;
	}

	return 0;
}

static inline struct snode* stree_get_node(struct stree* tree, uint32_t edgehi, uint16_t edgelo){
	return tree->index->buffer[edgehi]->buffer + edgelo;
}

static void stree_node_link_parent(struct stree* tree, struct snode* node, uint16_t edgelo){
	struct snode* parent;

	if (node->parenthi == INVALID_EDGE_HI){
		tree->root.edgelo[node->ptr[0]] = edgelo;
	}
	else {
		parent = stree_get_node(tree, node->parenthi, node->parentlo);
		parent->nb_child++;
		parent->edgelo[node->ptr[0]] = edgelo;
	}
}

static void stree_node_unlink_parent(struct stree* tree, struct snode* node){
	struct snode* parent;

	if (node->parenthi == INVALID_EDGE_HI){
		tree->root.edgelo[node->ptr[0]] = INVALID_EDGE_LO;
	}
	else {
		parent = stree_get_node(tree, node->parenthi, node->parentlo);
		parent->edgelo[node->ptr[0]] = INVALID_EDGE_LO;
		parent->nb_child--;
	}
}

static void stree_node_relink_parent(struct stree* tree, struct snode* node, uint16_t edgelo){
	struct snode* parent;

	if (node->parenthi == INVALID_EDGE_HI){
		tree->root.edgelo[node->ptr[0]] = edgelo;
	}
	else {
		parent = stree_get_node(tree, node->parenthi, node->parentlo);
		parent->edgelo[node->ptr[0]] = edgelo;
	}
}

static void stree_node_relink_childs(struct stree* tree, struct snode* node, uint32_t edgehi, uint16_t edgelo){
	uint32_t i;
	struct snode* child;

	for (i = 0; i < 0x100; i++){
		if (node->edgelo[i] != INVALID_EDGE_LO){
			child = stree_get_node(tree, node->edgehi, node->edgelo[i]);
			child->parenthi = edgehi;
			child->parentlo = edgelo;
		}
	}
}

static void stree_node_movlo(struct stree* tree, struct snode* node, uint32_t edgehi, uint16_t edgelo){
	stree_node_relink_parent(tree, node, edgelo);
	stree_node_relink_childs(tree, node, edgehi, edgelo);
}

static int stree_split_node(struct stree* tree, uint32_t edgehi, uint16_t edgelo, size_t split_size){
	uint32_t newedgehi;
	uint16_t newedgelo;
	struct snode* node;
	struct snode* new_node;

	if ((newedgehi = stree_reserve_edges(tree)) == INVALID_EDGE_HI){
		return -1;
	}

	if ((newedgelo = stree_alloc_edge(tree, newedgehi, 0)) == INVALID_EDGE_LO){
		return -1;
	}

	node = stree_get_node(tree, edgehi, edgelo);
	new_node = stree_get_node(tree, newedgehi, newedgelo);

	memcpy(new_node, node, sizeof(struct snode));

	new_node->parenthi = edgehi;
	new_node->parentlo = edgelo;
	new_node->size -= split_size;
	new_node->ptr += split_size;

	stree_node_relink_childs(tree, new_node, newedgehi, newedgelo);

	node->nb_child = 1;
	node->edgehi = newedgehi;
	memset(node->edgelo, 0xff, sizeof node->edgelo);
	node->edgelo[node->ptr[split_size]] = newedgelo;
	node->size = split_size;

	return 0;
}

int stree_insert(struct stree* tree, const uint8_t* ptr, size_t size){
	uint32_t parenthi;
	uint16_t parentlo;
	uint32_t edgehi;
	uint16_t edgelo;
	size_t offset;
	struct snode* node;
	size_t common_size;

	if (!size){
		return 0;
	}

	for (parenthi = INVALID_EDGE_HI, parentlo = INVALID_EDGE_LO, edgehi = 0, edgelo = tree->root.edgelo[ptr[0]], offset = 0; ; edgehi = node->edgehi, edgelo = node->edgelo[ptr[offset]]){
		if (edgelo == INVALID_EDGE_LO){
			uint16_t edgelo_new;

			if ((edgelo_new = stree_alloc_edge(tree, edgehi, 1)) == INVALID_EDGE_LO){
				return -1;
			}

			node = stree_get_node(tree, edgehi, edgelo_new);
			node->nb_child = 0;
			node->parenthi = parenthi;
			node->parentlo = parentlo;
			node->ptr = ptr + offset;
			node->size = size - offset;
			node->hit_size = 0;
			memset(node->edgelo, 0xff, sizeof node->edgelo);

			stree_node_link_parent(tree, node, edgelo_new);

			return 0;
		}

		node = stree_get_node(tree, edgehi, edgelo);
		parenthi = edgehi;
		parentlo = edgelo;

		common_size = get_common_size(node->ptr, ptr + offset, node->size, size - offset);

		if (offset + common_size == size){
			return 0;
		}

		if (common_size < node->size){
			if (stree_split_node(tree, edgehi, edgelo, common_size)){
				return -1;
			}
			node = stree_get_node(tree, edgehi, edgelo);
		}
		else if (!node->nb_child){
			node->ptr = ptr + offset;
			node->size = size - offset;
			return 0;
		}

		offset += common_size;
	}

	return 0;
}

int stree_insert_all(struct stree* tree, const uint8_t* ptr, size_t size, size_t min_size){
	size_t i;
	int status;

	if (size < min_size){
		return 0;
	}

	for (i = 0; i <= size - min_size; i++){
		if ((status = stree_insert(tree, ptr + i, size - i))){
			break;
		}
	}

	return status;
}

int stree_intersect(struct stree* tree, const uint8_t* ptr, size_t size){
	uint32_t edgehi;
	uint16_t edgelo;
	size_t offset;
	struct snode* node;
	size_t common_size;

	if (!size){
		return 0;
	}

	for (edgehi = 0, edgelo = tree->root.edgelo[ptr[0]], offset = 0; ; edgehi = node->edgehi, edgelo = node->edgelo[ptr[offset]]){
		if (edgelo == INVALID_EDGE_LO){
			return 0;
		}

		node = stree_get_node(tree, edgehi, edgelo);

		common_size = get_common_size(node->ptr, ptr + offset, node->size, size - offset);

		if (common_size > node->hit_size){
			node->hit_size = common_size;
		}

		if (offset + common_size == size){
			return 1;
		}

		if (common_size < node->size){
			return 0;
		}

		offset += common_size;
	}

	return 0;
}

int stree_intersect_all(struct stree* tree, const uint8_t* ptr, size_t size, size_t min_size){
	size_t i;

	if (size >= min_size){
		for (i = 0; i <= size - min_size; i++){
			stree_intersect(tree, ptr + i, size - i);
		}
	}

	return 0;
}

void stree_prune(struct stree* tree){
	uint32_t i;
	uint32_t j;
	uint32_t k;
	struct snode* node_j;
	struct snode* node_k;

	if (tree->index == NULL){
		return;
	}

	for (i = 0; i <= tree->index->current; i++){
		if (tree->index->buffer[i] == NULL){
			continue;
		}

		for (j = 0; j < tree->index->buffer[i]->nb_used; j++){
			node_j = stree_get_node(tree, i, j);
			if (!node_j->hit_size){
				stree_node_unlink_parent(tree, node_j);
			}
		}
	}

	for (i = 0; i <= tree->index->current; i++){
		if (tree->index->buffer[i] == NULL){
			continue;
		}

		for (j = 0, k = tree->index->buffer[i]->nb_used; j < k; j++){
			node_j = stree_get_node(tree, i, j);
			node_j->size = node_j->hit_size;
			node_j->hit_size = 0;
			if (!node_j->size){
				for (k--; k != j; k-- ){
					node_k = stree_get_node(tree, i, k);
					node_k->size = node_k->hit_size;
					node_k->hit_size = 0;
					if (node_k->size){
						memcpy(node_j, node_k, sizeof(struct snode));
						stree_node_movlo(tree, node_j, i, j);
						break;
					}
				}
			}
		}
		tree->index->buffer[i]->nb_used = k;
		tree->index->buffer[i] = index_dealloc(tree->index->buffer[i]);
	}
}

void stree_clean(struct stree* tree){
	if (tree->index != NULL){
		indexhi_clean(tree->index);
		free(tree->index);
	}
}

static int stree_iterator_forward(struct stree_iterator* sti, struct stree* tree){
	struct snode* node;
	uint32_t i;

	node = stree_get_node(tree, sti->edgehi, sti->edgelo);

	if (node->size > (sti->alloc_size - sti->offset)){
		size_t size_increase;
		uint8_t* new_ptr;

		size_increase = (4096 > node->size - (sti->alloc_size - sti->offset)) ? 4096 : node->size - (sti->alloc_size - sti->offset);
		if ((new_ptr = realloc(sti->ptr, sti->alloc_size + size_increase)) == NULL){
			return -1;
		}

		sti->ptr = new_ptr;
		sti->alloc_size += size_increase;
	}

	memcpy(sti->ptr + sti->offset, node->ptr, node->size);
	sti->offset += node->size;

	if (!node->nb_child){
		return 0;
	}

	for (i = 0; i < 0x100; i++){
		if (node->edgelo[i] != INVALID_EDGE_LO){
			sti->edgehi = node->edgehi;
			sti->edgelo = node->edgelo[i];

			return stree_iterator_forward(sti, tree);
		}
	}

	return -1;
}

static int stree_iterator_backward(struct stree_iterator* sti, struct stree* tree){
	struct snode* node;
	struct snode* parent;
	uint32_t i;

	node = stree_get_node(tree, sti->edgehi, sti->edgelo);

	sti->offset -= node->size;

	if (node->parenthi == INVALID_EDGE_HI){
		for (i = sti->ptr[0] + 1; i < 0x100; i++){
			if (tree->root.edgelo[i] != INVALID_EDGE_LO){
				sti->edgehi = 0;
				sti->edgelo = tree->root.edgelo[i];

				return stree_iterator_forward(sti, tree);
			}
		}

		return -1;
	}

	parent = stree_get_node(tree, node->parenthi, node->parentlo);
	for (i = sti->ptr[sti->offset] + 1; i < 0x100; i++){
		if (parent->edgelo[i] != INVALID_EDGE_LO){
			sti->edgehi = parent->edgehi;
			sti->edgelo = parent->edgelo[i];

			return stree_iterator_forward(sti, tree);
		}
	}

	sti->edgehi = node->parenthi;
	sti->edgelo = node->parentlo;

	return stree_iterator_backward(sti, tree);
}

static int stree_iterator_get_first(struct stree_iterator* sti, struct stree* tree){
	uint32_t i;

	for (i = 0; i < 0x100; i++){
		if (tree->root.edgelo[i] != INVALID_EDGE_LO){
			sti->edgehi = 0;
			sti->edgelo = tree->root.edgelo[i];

			return stree_iterator_forward(sti, tree);
		}
	}

	return -1;
}

int stree_iterator_init(struct stree_iterator* sti, struct stree* tree){
	sti->ptr = NULL;
	sti->alloc_size = 0;
	sti->offset = 0;

	return stree_iterator_get_first(sti, tree);
}

int stree_iterator_next(struct stree_iterator* sti, struct stree* tree){
	return stree_iterator_backward(sti, tree);
}

void stree_iterator_clean(struct stree_iterator* sti){
	free(sti->ptr);
}
