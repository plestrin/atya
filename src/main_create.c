#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "simple_index.h"
#include "abs_storage.h"
#include "gory_sewer.h"
#include "utile.h"

static int init(struct abs_storage* as, struct simple_index** si_ptr, size_t size){
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

	if ((status = abs_storage_create_head(as, si))){
		return status;
	}

	if ((status = abs_storage_intersect_tail(as, si))){
		return status;
	}

	return 0;
}

static int next(struct simple_index* si, struct abs_storage* as, struct simple_index** si_next_ptr){
	struct simple_index* si_next;
	int status;

	si_next = *si_next_ptr;
	if (si_next == NULL){
		if ((si_next = simple_index_create(si->size + 1)) == NULL){
			return ENOMEM;
		}
		*si_next_ptr = si_next;
	}

	si_next->size = si->size + 1;

	if ((status = abs_storage_derive_head(as, si, si_next))){
		return status;
	}

	if ((status = abs_storage_intersect_tail(as, si_next))){
		return status;
	}

	return 0;
}

#define START 6
#define STOP 16384

static void print_status(unsigned int flags, uint64_t pattern_cnt, uint64_t item_cnt, size_t size){
	if (flags & CMD_FLAG_VERBOSE && !((size - START) % 24)){
		fprintf(stderr, "\r%lu pattern(s) found, index size: %lu @ iteration %-10zu", pattern_cnt, item_cnt, size);
		fflush(stderr);
	}
}

static void print_status_final(unsigned int flags, uint64_t pattern_cnt, uint64_t item_cnt, size_t size){
	if (flags & CMD_FLAG_VERBOSE){
		fprintf(stderr, "\r%lu pattern(s) found, index size: %lu @ iteration %-10zu\n", pattern_cnt, item_cnt, size);
	}
}

static int create(struct gory_sewer_knob* file_gsk, unsigned int flags){
	struct simple_index* si_buffer[2] = {NULL, NULL};
	int si_index;
	struct abs_storage as;
	int status;
	uint64_t cnt = 0;

	if ((status = abs_storage_init(&as, file_gsk))){
		fprintf(stderr, "[-] in %s, unable to initialize abs storage\n", __func__);
		return status;
	}

	if ((status = init(&as, si_buffer, START))){
		fprintf(stderr, "[-] in %s, unable to create first simple index\n", __func__);
		goto exit;
	}

	for (si_index = 0; si_buffer[si_index]->nb_item; si_index = (si_index + 1) & 0x1){
		if (si_buffer[si_index]->size > STOP){
			log_info(flags, "reached max pattern size: %u", STOP)
			cnt += simple_index_dump_and_clean(si_buffer[si_index], STATUS_NONE, stdout);
			break;
		}

		print_status(flags, cnt, si_buffer[si_index]->nb_item, si_buffer[si_index]->size);

		if ((status = next(si_buffer[si_index], &as, si_buffer + ((si_index + 1) & 0x1)))){
			fprintf(stderr, "[-] in %s, unable to create index of size %zu\n", __func__, si_buffer[si_index]->size + 1);
			break;
		}

		cnt += simple_index_dump_and_clean(si_buffer[si_index], STATUS_NONE, stdout);
	}

	print_status_final(flags, cnt, si_buffer[si_index]->nb_item, si_buffer[si_index]->size);

	exit:

	abs_storage_clean(&as);

	if (si_buffer[0] != NULL){
		simple_index_delete(si_buffer[0]);
	}
	if (si_buffer[1] != NULL){
		simple_index_delete(si_buffer[1]);
	}

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
		if (create(file_gsk, flags)){
			status = EXIT_FAILURE;
		}
	}

	gory_sewer_delete(file_gsk);

	return status;
}
