#ifndef _MAXMIN_H_
#define _MAXMIN_H_

#include "types.h"
void read_file(char const *file, char**output);
void network_free(struct network_t *network);
void network_slo_violation(struct network_t *network);

#endif
