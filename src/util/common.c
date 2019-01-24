#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "util/common.h"

unsigned long
djb2_hash(unsigned char const *str) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++) != 0)
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

unsigned int upper_pow2(unsigned int v) {
  v--;
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  v++;

  return v;
}

long get_ncores(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

int dir_exists(char const *dname) {
  struct stat st = {0};
  return stat(dname, &st) != -1;
}

int dir_mk(char const *dname) {
  struct stat st = {0};
  if (stat(dname, &st) == -1) {
    mkdir(dname, 0700);
  }
  return 1;
}
