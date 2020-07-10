#include <stdlib.h>
#include <stdio.h>

#include "last_index.h"

void* read_file_standard(const char* file_path, size_t* size){
	FILE* file;
	void* data = NULL;
	uint64_t file_size;

	if ((file = fopen(file_path, "r")) == NULL){
		fprintf(stderr, "[-] in %s, unable open file %s\n", __func__, file_path);
		return NULL;
	}

	do {
		if (fseek(file, 0, SEEK_END) < 0){
			fprintf(stderr, "[-] in %s, unable to seek end of file\n", __func__);
			break;
		}

		file_size = ftell(file);
		if (file_size >> 32){
			fprintf(stderr, "[-] in %s, file is too large (0x%lx)\n", __func__, file_size);
			break;
		}

		if (fseek(file, 0, SEEK_SET) < 0){
			fprintf(stderr, "[-] in %s, unable to seek start of file\n", __func__);
			break;
		}

		if ((data = malloc(file_size)) == NULL){
			fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
			break;
		}

		*size = fread(data, 1, file_size, file);
		if (*size != file_size){
			fprintf(stderr, "[-] in %s, incomplete read: 0x%lx byte(s) are missing\n", __func__, file_size - *size);
		}
	} while (0);

	fclose(file);

	return data;
}

struct last_index li;

#define MIN_SIZE 5
#define PUSH_SIZE 23

int main(int argc, char** argv){
	uint8_t* file_data;
	size_t file_size;
	uint64_t i;

	if (argc != 2){
		fprintf(stderr, "[-] in %s, usage: %s filename\n", __func__, argv[0]);
		return EXIT_FAILURE;
	}

	if ((file_data = read_file_standard(argv[1], &file_size)) == NULL){
		fprintf(stderr, "[-] in %s, unable to read file %s\n", __func__, argv[1]);
		return EXIT_FAILURE;
	}

	last_index_init(&li, MIN_SIZE);

	for (i = 0; i + PUSH_SIZE <= file_size; i++){
		last_index_push(&li, file_data + i, PUSH_SIZE);
	}

	last_index_exclude_file(&li, argv[1]);

	for (i = 0; i < 0x10000; i++){
		if (li.index[i] != NULL){
			fprintf(stderr, "[-] in %s, entry %lu is not empty\n", __func__, i);
		}
	}

	last_index_clean(&li);

	free(file_data);

	return EXIT_SUCCESS;
}
