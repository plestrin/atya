#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "stree.h"
#include "simple_index.h"
#include "reverse_index.h"
#include "abs_storage.h"
#include "gory_sewer.h"
#include "utile.h"

static int make_simple_index(struct abs_storage* as, struct simple_index** si_ptr, size_t size){
	struct simple_index* si;
	int status;

	si = *si_ptr;
	if (si == NULL){
		if ((si = simple_index_create(size)) == NULL){
			return ENOMEM;
		}
		*si_ptr = si;
	}

	si->size = size;

	if ((status = abs_storage_fill_simple_index_head(as, si))){
		return status;
	}

	if ((status = abs_storage_intersect_simple_index_tail(as, si))){
		return status;
	}

	return 0;
}

static int make_stree(struct abs_storage* as, struct stree* tree, size_t min_size){
	struct simple_index* si;
	int status;

	if ((status = stree_init(tree))){
		fprintf(stderr, "[-] in %s, unable to initialize suffix tree\n", __func__);
		return status;
	}

	if ((status = make_simple_index(as, &si, min_size))){
		return status;
	}

	if (!(status = abs_storage_fill_stree_head(as, tree, si))){
		status = abs_storage_intersect_stree_tail(as, tree, si);
	}

	simple_index_delete(si);

	return status;
}

static int dump_stree(struct stree* tree, size_t min_size){
	struct stree_iterator sti;
	int end;
	struct reverse_index* ri;
	int status;

	if ((ri = reverse_index_create(min_size)) == NULL){
		return ENOMEM;
	}

	for (end = stree_iterator_init(&sti, tree); !end; end = stree_iterator_next(&sti, tree)){
		if ((status = reverse_index_append(ri, sti.ptr, sti.offset))){
			fprintf(stderr, "[-] in %s, unable to append data to reverse index\n", __func__);
			break;
		}
	}

	stree_iterator_clean(&sti);

	reverse_index_dump(ri, stdout);

	reverse_index_delete(ri);

	return status;
}

#define START 6

static int create(struct gory_sewer_knob* file_gsk){
	struct abs_storage as;
	int status;
	struct stree tree;

	if ((status = abs_storage_init(&as, file_gsk))){
		fprintf(stderr, "[-] in %s, unable to initialize abs storage\n", __func__);
		return status;
	}

	if ((status = make_stree(&as, &tree, START))){
		fprintf(stderr, "[-] in %s, unable to make suffix tree\n", __func__);
	}
	else {
		if ((status = dump_stree(&tree, START))){
			fprintf(stderr, "[-] in %s, unable to dump suffix tree\n", __func__);
		}
	}

	stree_clean(&tree);
	abs_storage_clean(&as);

	return status;
}

int main(int argc, char** argv){
	struct gory_sewer_knob* file_gsk;
	unsigned int flags;
	int status = EXIT_SUCCESS;

	if (parse_cmd_line(argc, argv, &file_gsk, &flags)){
		return EXIT_FAILURE;
	}

	if (!(flags & CMD_FLAG_NOOP)){
		if (create(file_gsk)){
			status = EXIT_FAILURE;
		}
	}

	gory_sewer_delete(file_gsk);

	return status;
}
