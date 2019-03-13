#ifndef _UTIL_COMMON_H_
#define _UTIL_COMMON_H_

#include <stdio.h>

#ifdef linux
#include <linux/limits.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

#ifdef _WIN32
#define PATH_SEPARATOR   "\\"
#else
#define PATH_SEPARATOR   "/"
#endif


#define INT_MAX_LEN 20

// Max number of steps that EWMA will predict
#define EWMA_MAX_TM_STRIDE 10

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

#define DURATION_SEPARATOR ':'

/* Get the number of cores */
long get_ncores(void);

/* Directory functions */
int dir_exists(char const *dname);
int dir_mk(char const *dname);
int dir_num_files(char const *dname);

int fd_path(int, char **);
size_t file_read(FILE *, char **);

#endif
