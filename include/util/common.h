#ifndef _UTIL_COMMON_H_
#define _UTIL_COMMON_H_

#include <stdio.h>
#include <stdint.h>

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

// Max number of steps that EWMA will predict.  Used in rotating ewma
// decleration.
#define EWMA_MAX_TM_STRIDE 10

// http://www.cse.yorku.ca/~oz/hash.html
unsigned long
djb2_hash(unsigned char const *str);

// Round up an int to the nearest power of two that is larger
unsigned int upper_pow2(unsigned int v);

// Set the current string hash to djb2
#define HASH djb2_hash

/* TODO: These are only being used in traffic.c and rotating_ewma.c
 * I can probably refactor these: use them everywhere or remove them from those
 * two files.
 *
 * - Omid 3/27/2019
 */
#define SUCCESS 1
#define FAILURE 0

/* MAX and MIN definition using macros */
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))

/* Get the number of cores */
long get_ncores(void);

/* Directory functions */

/* Check if a directory exists */
int dir_exists(char const *dname);

/* Create a directory if it doesn't exist. */
void dir_mk(char const *dname);

/* Returns the number of files in the director */
uint32_t dir_num_files(char const *dname);

/* Returns the path to a file given a file descriptor */
int fd_path(int, char **);

/* Read a file in one go and put the results in the char ** */
size_t file_read(FILE *, char **);

#endif
