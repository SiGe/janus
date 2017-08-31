#ifndef _ALGORITHM_H_
#define _ALGORITHM_H_

#include "types.h"

/* Returns the flows under max-min fairness */
int maxmin(struct network_t *network);

int **generate_subplan(int *groups, int groups_len, int *subplan_nums);

int *double_ewma(int *seq, int seq_len, double alpha, int n, double beta);

#endif
