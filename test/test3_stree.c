#include <stdlib.h>
#include <stdio.h>

#include "stree.h"
#include "utile.h"

static void stree_print_stat(struct stree* tree){
	size_t mem;
	uint32_t i;
	uint32_t nb_indexhi;
	uint32_t nb_node;

	if (tree->index == NULL){
		return;
	}

	mem = sizeof(struct indexhi) + sizeof(struct indexlo*) * tree->index->nb_alloc;

	for (i = 0, nb_indexhi = 0, nb_node = 0; i < tree->index->nb_alloc; i++){
		if (tree->index->buffer[i] != NULL){
			nb_node += tree->index->buffer[i]->nb_alloc;
			mem += sizeof(struct indexlo*) + sizeof(struct snode) * tree->index->buffer[i]->nb_alloc;
			nb_indexhi ++;
		}
	}

	fprintf(stderr, "Memory: %zu\n", mem);
	fprintf(stderr, "Nb index hi: %u\n", nb_indexhi);
	fprintf(stderr, "Nb node: %u (node per index hi: %u)\n", nb_node, nb_node / nb_indexhi);
}

int main(int argc, char** argv){
	int status = EXIT_SUCCESS;
	struct stree tree;
	uint8_t* data;
	size_t size;

	if (argc != 2){
		fprintf(stderr, "[-] usage: %s file\n", argv[0]);
		return EXIT_FAILURE;
	}

	if (load_file(argv[1], &data, &size)){
		fprintf(stderr, "[-] cannot load file %s\n", argv[1]);
		return EXIT_FAILURE;
	}

	if (stree_init(&tree)){
		fprintf(stderr, "[-] cannot init suffix tree\n");
		status = EXIT_FAILURE;
	}
	else {
		struct stree_iterator sti;
		int end;

		if (stree_insert_all(&tree, data, size, 1)){
			fprintf(stderr, "[-] cannot insert\n");
		}

		for (end = stree_iterator_init(&sti, &tree); !end; end = stree_iterator_next(&sti, &tree));

		stree_iterator_clean(&sti);

		stree_print_stat(&tree);
	}

	stree_clean(&tree);

	free(data);

	return status;
}
