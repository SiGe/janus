#ifndef _ALGORITHM_H_
#define _ALGORITHM_H_

#include "types.h"

/* Returns the flows under max-min fairness */
int maxmin(struct network_t *network);

int **generate_subplan(int *groups, int groups_len, int *subplan_nums);

bw_t *double_ewma(bw_t *seq, int seq_len, double alpha, int n, double beta);

static inline bw_t max(bw_t a, bw_t b) {
  return  (a > b) ? a : b;
}

#endif
