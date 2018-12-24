#include "util/common.h"

unsigned long
djb2_hash(unsigned char const *str) {
  unsigned long hash = 5381;
  int c;

  while ((c = *str++) != 0)
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}
