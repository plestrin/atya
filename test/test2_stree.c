#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "stree.h"

#define MIN_SIZE 3

int main(int argc, char** argv){
	int status = EXIT_SUCCESS;
	struct stree tree;
	int i;
	struct stree_iterator sti;
	int end;

	if (stree_init(&tree)){
		fprintf(stderr, "[-] cannot init suffix tree\n");
		status = EXIT_FAILURE;
	}
	else if (argc > 1){
		if (stree_insert_all(&tree, (uint8_t*)argv[1], strlen(argv[1]), MIN_SIZE)){
			fprintf(stderr, "[-] cannot insert\n");
			status = EXIT_FAILURE;
		}
		else {
			for (i = 2; i < argc; i++){
				if (stree_intersect_all(&tree, (uint8_t*)argv[i], strlen(argv[i]), MIN_SIZE)){
					fprintf(stderr, "[-] cannot intersect\n");
					status = EXIT_FAILURE;
					break;
				}

				stree_prune(&tree);
			}

			for (end = stree_iterator_init(&sti, &tree); !end; end = stree_iterator_next(&sti, &tree)){
				fwrite(sti.ptr, sti.offset, 1, stdout);
				fputc('\n', stdout);
			}

			stree_iterator_clean(&sti);
		}
	}

	stree_clean(&tree);

	return status;
}
