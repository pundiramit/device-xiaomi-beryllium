#ifndef _HASH_H_
#define _HASH_H_

#include <stdint.h>

unsigned int hash_mem(const void *data, unsigned int len);
unsigned int hash_string(const char *value);
unsigned int hash_u32(uint32_t value);
unsigned int hash_u64(uint64_t value);
unsigned int hash_pointer(void *value);

#endif
