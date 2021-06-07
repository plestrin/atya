#ifndef UTILE_H
#define UTILE_H

#include <stdlib.h>
#include <stdint.h>

#include "gory_sewer.h"

#define CMD_FLAG_VERBOSE 0x01
#define CMD_FLAG_NOOP 0x02

int parse_cmd_line(int argc, char** argv, struct gory_sewer_knob** file_gsk_ptr, unsigned int* flags_ptr);

int load_file(const char* file_name, uint8_t** data_ptr, size_t* size_ptr);

#define log_info(flags, M, ...) 						\
	if (flags & CMD_FLAG_VERBOSE) { 					\
		fprintf(stderr, "[+] " M "\n", ##__VA_ARGS__); 	\
	}


#endif
