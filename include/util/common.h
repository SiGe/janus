#ifndef _COMMON_H_
#define _COMMON_H_

#ifdef linux
#include <linux/limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// http://www.cse.yorku.ca/~oz/hash.html
unsigned long
djb2_hash(unsigned char const *str);

// Set the current string hash to djb2
#define HASH djb2_hash

#endif
