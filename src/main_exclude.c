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

	fprintf(stderr, "[+] starting exclusion with %lu patterns\n", li->nb_item);

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
		fprintf(stderr, "[-] in %s, unable to init gory sewers\n", __func__);
		return NULL;
	}

	for (i = 1; i < argc; i++){
		strncpy(path, argv[i], sizeof path);
		path[sizeof path - 1] = 0;
		if (list_files(path, sizeof path, file_gsk)){
			fprintf(stderr, "[-] in %s, unable to list files in: %s\n", __func__, path);
		}
	}

	fprintf(stderr, "[+] command line: %lu file(s)\n", file_gsk->nb_item);

	return file_gsk;
}

#define START 6
#define STOP 4096

static struct last_index li;

int main(int argc, char** argv){
	struct gory_sewer_knob* file_gsk;
	uint64_t size;
	uint8_t pattern[STOP];

	if ((file_gsk = parse_cmd_line(argc, argv)) == NULL){
		return EXIT_FAILURE;
	}

	last_index_init(&li, START);

	while (fread(&size, sizeof size, 1, stdin)){
		if (size < START){
			fprintf(stderr, "[-] in %s, pattern %lu is too small: %lu\n", __func__, li.nb_item, size);
			break;
		}
		if (size > STOP){
			fprintf(stderr, "[-] in %s, pattern %lu is too large: %lu\n", __func__, li.nb_item, size);
			break;
		}

		if (fread(pattern, size, 1, stdin) != 1){
			fprintf(stderr, "[-] in %s, unable to read pattern\n", __func__);
			break;
		}

		if (last_index_push(&li, pattern, size)){
			fprintf(stderr, "[-] in %s, unable to push data to last index\n", __func__);
		}
	}


	last_exclude_files(&li, file_gsk);
	last_index_dump_and_clean(&li, stdout);

	gory_sewer_delete(file_gsk);

	return EXIT_SUCCESS;
}
