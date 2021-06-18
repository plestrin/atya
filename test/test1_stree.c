#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "stree.h"

static const char HEX[] = "0123456789abcdef";

static void print_data(const uint8_t* ptr, size_t size){
	size_t i;

	for (i = 0; i < size; i++){
		if (ptr[i] >= 0x20 && ptr[i] <= 0x7e){
			fputc(ptr[i], stdout);
		}
		else {
			fprintf(stdout, "\\x%x%c", HEX[ptr[i] >> 4], HEX[ptr[i] & 0xf]);
		}
	}
}

static inline struct snode* stree_get_node(struct stree* tree, uint32_t edgehi, uint16_t edgelo){
	return tree->index->buffer[edgehi]->buffer + edgelo;
}

static void stree_print_node(struct stree* tree, uint32_t edgehi, uint16_t edgelo, uint32_t depth){
	struct snode* node;
	uint32_t i;

	node = stree_get_node(tree, edgehi, edgelo);

	for (i = 0; i< depth; i++){
		fputs("\t", stdout);
	}

	print_data(node->ptr, node->size);


	if (!node->nb_child){
		fputs(" LEAF\n", stdout);
	}
	else {
		fputs(" :\n", stdout);
	}

	for (i = 0; i < 0x100; i++){
		if (node->edgelo[i] != 0xffff){
			stree_print_node(tree, node->edgehi, node->edgelo[i], depth + 1);
		}
	}
}

static void stree_print(struct stree* tree){
	uint32_t i;

	for (i = 0; i < 0x100; i++){
		if (tree->root.edgelo[i] != 0xffff){
			stree_print_node(tree, 0, tree->root.edgelo[i], 0);
		}
	}
}

static void stree_check(struct stree* tree){
	uint32_t i;
	uint32_t j;
	uint32_t k;
	uint32_t cnt;
	struct snode* node;
	struct snode* child;

	if (tree->index == NULL){
		return;
	}

	for (i = 0; i <= tree->index->current; i++){
		if (tree->index->buffer[i] == NULL){
			continue;
		}

		for (j = 0; j < tree->index->buffer[i]->nb_used; j++){
			node = stree_get_node(tree, i, j);

			if (node->parenthi == 0xffffffff){
				if (i || node->parentlo != 0xffff){
					fprintf(stderr, "[-] parent error\n");
				}
				if (tree->root.edgelo[node->ptr[0]] != i){
					fprintf(stderr, "[-] broken link @ root\n");
				}
			}

			if (!node->size){
				fprintf(stderr, "[-] size error\n");
			}

			for (k = 0, cnt = 0; k < 0x100; k++){
				if (node->edgelo[k] != 0xffff){
					cnt ++;
					child = stree_get_node(tree, node->edgehi, node->edgelo[k]);
					if (child->parenthi != i){
						fprintf(stderr, "[-] broken link hi: %u != %u\n", child->parenthi, i);
					}
					if (child->parentlo != j){
						fprintf(stderr, "[-] broken link lo: %u != %u\n", child->parentlo, j);
					}
					if (child->ptr[0] != k){
						fprintf(stderr, "[-] broken link value\n");
					}
				}
			}

			if (cnt != node->nb_child){
				fprintf(stderr, "[-] child count error\n");
			}
		}
	}
}

int main(int argc, char** argv){
	int status = EXIT_SUCCESS;
	struct stree tree;
	int i;

	printf("[+] size of struct snode: %zu\n", sizeof(struct snode));

	if (stree_init(&tree)){
		fprintf(stderr, "[-] cannot init suffix tree\n");
		status = EXIT_FAILURE;
	}
	else {
		for (i = 1; i < argc; i++){
			if (stree_insert(&tree, (uint8_t*)argv[i], strlen(argv[i]))){
				fprintf(stderr, "[-] cannot insert\n");
				status = EXIT_FAILURE;
				break;
			}
		}

		stree_check(&tree);

		stree_print(&tree);
	}


	stree_clean(&tree);

	return status;
}
