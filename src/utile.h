#ifndef UTILE_H
#define UTILE_H

#include <stdlib.h>
#include <stdint.h>

#include "gory_sewer.h"

int list_files(char* path, size_t max_len, struct gory_sewer_knob* gsk);

int load_file(const char* file_name, uint8_t** data_ptr, size_t* size_ptr);

#endif
