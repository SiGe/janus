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

