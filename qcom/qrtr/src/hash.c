#include <string.h>
#include "hash.h"

unsigned int hash_mem(const void *data, unsigned int len)
{
	unsigned int h;
	unsigned int i;

	h = len;

	for (i = 0; i < len; ++i)
		h = ((h >> 27) ^ (h << 5)) ^ ((const unsigned char *)data)[i];

	return h;
}

unsigned int hash_string(const char *value)
{
	return hash_mem(value, strlen(value));
}

unsigned int hash_u32(uint32_t value)
{
	return value * 2654435761UL;
}

unsigned int hash_u64(uint64_t value)
{
	return hash_u32(value & 0xffffffff) ^ hash_u32(value >> 32);
}

unsigned int hash_pointer(void *value)
{
	if (sizeof(value) == sizeof(uint64_t))
		return hash_u64((long)value);
	return hash_u32((long)value);
}
