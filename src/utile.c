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

int list_files(char* path, size_t max_len, struct gory_sewer_knob* gsk){
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

		return gory_sewer_push(gsk, path, len + 1);
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
