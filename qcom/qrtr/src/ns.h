#ifndef __NS_H_
#define __NS_H_

#include <endian.h>
#include <stdint.h>

static inline __le32 cpu_to_le32(uint32_t x) { return htole32(x); }
static inline uint32_t le32_to_cpu(__le32 x) { return le32toh(x); }

#endif
