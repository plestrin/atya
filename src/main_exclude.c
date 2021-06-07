#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "last_index.h"
#include "gory_sewer.h"
#include "utile.h"

static int last_exclude_files(struct last_index* li, struct gory_sewer_knob* gsk_files){
	char* file_path;
	int status = 0;

	for (file_path = gory_sewer_first(gsk_files); file_path != NULL && li->nb_item; file_path = gory_sewer_next(gsk_files)){
		if ((status = last_index_exclude_file(li, file_path))){
			fprintf(stderr, "[-] in %s, unable to exclude file: %s\n", __func__, file_path);
		}
	}

	return status;
}

static struct last_index li;

static int exclude(struct gory_sewer_knob* file_gsk, unsigned int flags){
	struct gory_sewer_knob* pattern_gsk;
	uint64_t rd_size;
	void* pattern;
	uint64_t size_min = 0xffffffffffffffff;
	uint64_t size_max = 0;
	size_t it_size;
	int status = 0;

	if ((pattern_gsk = gory_sewer_create(0x10000)) == NULL){
		fprintf(stderr, "[-] in %s, unable to create gory sewer\n", __func__);
		return ENOMEM;
	}

	while (fread(&rd_size, sizeof rd_size, 1, stdin)){
		if (!rd_size){
			continue;
		}

		if ((pattern = gs_sl_alloc(pattern_gsk, rd_size)) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate %zu bytes in gory sewer\n", __func__, rd_size);
			status = ENOMEM;
			continue;
		}

		if (fread(pattern, rd_size, 1, stdin) != 1){
			status = errno;
			fprintf(stderr, "[-] in %s, unable to read pattern\n", __func__);
			continue;
		}

		if (rd_size < size_min){
			size_min = rd_size;
		}
		if (rd_size > size_max){
			size_max = rd_size;
		}
	}

	if (pattern_gsk->nb_item){
		log_info(flags, "%lu pattern(s) read, %lu <= size <= %lu", pattern_gsk->nb_item, size_min, size_max);

		last_index_init(&li, size_min);

		for (pattern = gs_sl_first(pattern_gsk, &it_size); pattern != NULL; pattern = gs_sl_next(pattern_gsk, &it_size)){
			if ((status = last_index_push(&li, pattern, it_size))){
				fprintf(stderr, "[-] in %s, unable to push data to last index\n", __func__);
			}
		}

		last_exclude_files(&li, file_gsk);

		log_info(flags, "%lu pattern(s) remaining", li.nb_item);

		last_index_dump_and_clean(&li, stdout);
	}

	gory_sewer_delete(pattern_gsk);

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
		if (exclude(file_gsk, flags)){
			status = EXIT_FAILURE;
		}
	}

	gory_sewer_delete(file_gsk);

	return status;
}
