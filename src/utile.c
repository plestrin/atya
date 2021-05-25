#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>

#include "utile.h"

static int list_files(char* path, size_t max_len, struct gory_sewer_knob* gsk){
	struct stat statbuf;
	int status;
	size_t len;

	len = strlen(path);
	if (lstat(path, &statbuf)){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to stat file %s\n", __func__, path);
		return status;
	}

	if (S_ISLNK(statbuf.st_mode)){
		return 0;
	}

	if (S_ISREG(statbuf.st_mode)){
		int file;

		if ((file = open(path, O_RDONLY)) < 0){
			status = errno;
			fprintf(stderr, "[-] in %s, unable to open file: %s\n", __func__, path);
			return status;
		}

		close(file);

		if (gory_sewer_push(gsk, path, len + 1) == NULL){
			fprintf(stderr, "[-] in %s, unable to add path to gory sewer\n", __func__);
			return -1;
		}

		return 0;
	}

	if (S_ISDIR(statbuf.st_mode)){
		DIR *d;
		struct dirent *dir;

		if ((d = opendir(path)) == NULL){
			status = errno;
			fprintf(stderr, "[-] in %s, unable to open directory: %s\n", __func__, path);
			return status;
		}

		if (len + 1 == max_len){
			fprintf(stderr, "[-] in %s, increase the PATH size\n", __func__);
			return EINVAL;
		}

		if (path[len - 1 ] != '/'){
			path[len ++] = '/';
		}

		while ((dir = readdir(d)) != NULL) {
			if (!strcmp(dir->d_name, ".") || !strcmp(dir->d_name, "..")){
				continue;
			}

			strncpy(path + len, dir->d_name, max_len - len);
			path[max_len - 1] = 0;

			if (list_files(path, max_len, gsk)){
				fprintf(stderr, "[-] in %s, unable to list files in: %s\n", __func__, path);
			}
		}

		closedir(d);
	}

	return 0;
}

static char* get_file_name(char* path){
	char* name;

	if ((name = strrchr(path, '/')) != NULL){
		return name + 1;
	}

	return path;
}

int parse_cmd_line(int argc, char** argv, struct gory_sewer_knob** file_gsk_ptr, unsigned int* flags_ptr){
	char path[4096];
	int i;

	if (argc < 2){
		fprintf(stderr, "[-] usage: %s [-v | --verbose] file [file ...]\n", get_file_name(argv[0]));
		return EINVAL;
	}

	*flags_ptr = 0;

	if ((*file_gsk_ptr = gory_sewer_create(0x4000)) == NULL){
		fprintf(stderr, "[-] in %s, unable to create gory sewer\n", __func__);
		return ENOMEM;
	}

	for (i = 1; i < argc; i++){
		if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose")){
			*flags_ptr = *flags_ptr | CMD_FLAG_VERBOSE;
			continue;
		}

		strncpy(path, argv[i], sizeof path);
		path[sizeof path - 1] = 0;
		if (list_files(path, sizeof path, *file_gsk_ptr)){
			fprintf(stderr, "[-] in %s, unable to list files in: %s\n", __func__, path);
		}
	}

	log_info(*flags_ptr, "%s command line: %lu file(s)", get_file_name(argv[0]), (*file_gsk_ptr)->nb_item);

	return 0;
}

int load_file(const char* file_name, uint8_t** data_ptr, size_t* size_ptr){
	int fd;
	struct stat statbuf;
	uint8_t* data = NULL;
	size_t size;
	ssize_t read_sz;
	int status = 0;

	if ((fd = open(file_name, O_RDONLY)) == -1){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to open \"%s\"\n", __func__, file_name);
		return status;
	}

	if (fstat(fd, &statbuf)){
		status = errno;
		fprintf(stderr, "[-] in %s, unable to stat file\n", __func__);
		goto exit;
	}

	size = statbuf.st_size;

	if ((data = malloc(size)) == NULL){
		status = ENOMEM;
		fprintf(stderr, "[-] in %s, unable to allocate memory\n", __func__);
		goto exit;
	}

	read_sz = read(fd, data, size);
	if (read_sz < 0){
		status = errno;
		fprintf(stderr, "[-] in %s, error while reading file\n", __func__);
		goto exit;
	}
	if ((size_t)read_sz < size){
		fprintf(stderr, "[-] in %s, incomplete read\n", __func__);
		size = read_sz;
	}

	*data_ptr = data;
	*size_ptr = size;

	exit:

	close(fd);

	if (status){
		free(data);
	}

	return status;
}
