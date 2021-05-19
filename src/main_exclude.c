#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "last_index.h"
#include "gory_sewer.h"
#include "utile.h"

static int last_exclude_files(struct last_index* li, struct gory_sewer_knob* gsk_files){
	char* file_path;
	int status = 0;

	for (file_path = gory_sewer_first(gsk_files); file_path != NULL; file_path = gory_sewer_next(gsk_files)){
		if ((status = last_index_exclude_file(li, file_path))){
			fprintf(stderr, "[-] in %s, unable to exclude file: %s\n", __func__, file_path);
			break;
		}
	}

	return status;
}

static struct gory_sewer_knob* parse_cmd_line(int argc, char** argv){
	char path[4096];
	struct gory_sewer_knob* file_gsk;
	int i;

	if (argc < 2){
		fprintf(stderr, "[-] usage: file [file ...]\n");
		return NULL;
	}

	if ((file_gsk = gory_sewer_create(0x4000)) == NULL){
		fprintf(stderr, "[-] in %s, unable to create gory sewer\n", __func__);
		return NULL;
	}

	for (i = 1; i < argc; i++){
		strncpy(path, argv[i], sizeof path);
		path[sizeof path - 1] = 0;
		if (list_files(path, sizeof path, file_gsk)){
			fprintf(stderr, "[-] in %s, unable to list files in: %s\n", __func__, path);
		}
	}

	fprintf(stderr, "[+] exclude command line: %lu file(s)\n", file_gsk->nb_item);

	return file_gsk;
}

static struct last_index li;

int main(int argc, char** argv){
	int status = EXIT_SUCCESS;
	struct gory_sewer_knob* file_gsk;
	struct gory_sewer_knob* pattern_gsk;


	if ((file_gsk = parse_cmd_line(argc, argv)) == NULL){
		return EXIT_FAILURE;
	}

	if ((pattern_gsk = gory_sewer_create(0x10000)) == NULL){
		fprintf(stderr, "[-] in %s, unable to create gory sewer\n", __func__);
		status = EXIT_FAILURE;
	}
	else {
		uint64_t rd_size;
		void* pattern;
		uint64_t size_min = 0xffffffffffffffff;
		uint64_t size_max = 0;
		size_t it_size;

		while (fread(&rd_size, sizeof rd_size, 1, stdin)){
			if (!rd_size){
				continue;
			}

			if ((pattern = gs_sl_alloc(pattern_gsk, rd_size)) == NULL){
				fprintf(stderr, "[-] in %s, unable to alloc %zu bytes in gory sewer\n", __func__, rd_size);
				status = EXIT_FAILURE;
				continue;
			}

			if (fread(pattern, rd_size, 1, stdin) != 1){
				fprintf(stderr, "[-] in %s, unable to read pattern\n", __func__);
				status = EXIT_FAILURE;
				break;
			}

			if (rd_size < size_min){
				size_min = rd_size;
			}
			if (rd_size > size_max){
				size_max = rd_size;
			}
		}

		fprintf(stderr, "[+] %lu pattern(s) read, %lu <= size <= %lu\n", pattern_gsk->nb_item, size_min, size_max);

		last_index_init(&li, size_min);

		for (pattern = gs_sl_first(pattern_gsk, &it_size); pattern != NULL; pattern = gs_sl_next(pattern_gsk, &it_size)){
			if (last_index_push(&li, pattern, it_size)){
				fprintf(stderr, "[-] in %s, unable to push data to last index\n", __func__);
				status = EXIT_FAILURE;
			}
		}

		last_exclude_files(&li, file_gsk);
		last_index_dump_and_clean(&li, stdout);

		gory_sewer_delete(pattern_gsk);
	}

	gory_sewer_delete(file_gsk);

	return status;
}
