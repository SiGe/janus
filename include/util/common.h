#ifndef _COMMON_H_
#define _COMMON_H_

#ifdef linux
#include <linux/limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#define INT_MAX_LEN 20

// http://www.cse.yorku.ca/~oz/hash.html
unsigned long
djb2_hash(unsigned char const *str);

unsigned int upper_pow2(unsigned int v);

// Set the current string hash to djb2
#define HASH djb2_hash
#define SUCCESS 1
#define FAILURE 0

#define METHOD(o, m, ...) ((o)->m(o, __VA_ARGS__))

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

long get_ncores(void);

#endif
