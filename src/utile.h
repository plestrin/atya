#ifndef UTILE_H
#define UTILE_H

#include <stdlib.h>
#include <stdint.h>

#include "gory_sewer.h"

int parse_cmd_line(int argc, char** argv, struct gory_sewer_knob** file_gsk_ptr);

int load_file(const char* file_name, uint8_t** data_ptr, size_t* size_ptr);

#endif
