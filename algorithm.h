#ifndef _ALGORITHM_H_
#define _ALGORITHM_H_

#include "types.h"

/* Returns the flows under max-min fairness */
int maxmin(struct network_t *network);

int **generate_subplan(int *groups, int groups_len);

#endif
