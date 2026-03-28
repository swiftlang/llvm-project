#if __wasi__
#include <string.h>
#include <stdint.h>
#include <endian.h>

void *__wasilibc_memcpy(void *restrict dest, const void *restrict src, size_t n)
{
	unsigned char *d = dest;
	const unsigned char *s = src;

	for (; n; n--) *d++ = *s++;
	return dest;
}
#endif
