#ifndef HASH_H
#define HASH_H

#include <stdlib.h>
#include <stdint.h>

uint16_t hash_init(const uint8_t* data, size_t size);
uint16_t hash_update(uint16_t hash, uint8_t old, uint8_t new, size_t size);

#endif
