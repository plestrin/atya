#include <stdlib.h>
#include <stdint.h>

static inline uint16_t rol16(uint16_t value, uint64_t disp){
	return (value << disp) | (value >> (16 - disp));
}

uint16_t hash_init(const uint8_t* data, size_t size){
	uint16_t hash;
	size_t i;

	for (i = 0, hash = 0; i < size; i ++){
		hash = rol16(hash, 3);
		hash ^= data[i];
	}

	return hash;
}

uint16_t hash_update(uint16_t hash, uint8_t old, uint8_t new, size_t size){
	hash = rol16(hash, 3);
	hash ^= rol16(old, (3 * size) & 0xf);
	return hash ^ new;
}

uint16_t hash_pop(uint16_t hash, uint8_t old){
	return rol16(hash ^ old, 13);
}

uint16_t hash_push(uint16_t hash, uint8_t new){
	return rol16(hash, 3) ^ new;
}
