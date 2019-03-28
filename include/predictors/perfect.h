#ifndef _PREDICTORS_PERFECT_H_
#define _PREDICTORS_PERFECT_H_

#include "predictor.h"

/* Perfect iterator returns accurate TMs from the real traffic trace */
struct predictor_perfect_iterator_t {
  struct predictor_iterator_t; /* Perfect predictor iterator is a predictor iterator */
  int done; /* Has the predictor iterator reached the end?? */
};

struct predictor_perfect_t {
  struct predictor_t; /* A perfect predictor is ... perfect */
  struct traffic_matrix_trace_t *trace; /* The real traffic trace */
};

/* Free the perfect predictor */
void predictor_perfect_free(struct predictor_t *);

/* Perfect predictor doesn't have a _create function.  You should call _load
 * with your trace object to create a new one */
struct predictor_perfect_t *
  predictor_perfect_load(struct traffic_matrix_trace_t *);

#endif // _PREDICTORS_EWMA_H_
