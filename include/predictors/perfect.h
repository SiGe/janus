#ifndef _PREDICTORS_PERFECT_H_
#define _PREDICTORS_PERFECT_H_

#include "predictor.h"

struct predictor_perfect_iterator_t {
  struct predictor_iterator_t;

  int done;
};

struct predictor_perfect_t {
  struct predictor_t;
  struct traffic_matrix_trace_t *trace;
};

/*
 * @param exp_coeff: Exponential smoothing factor
 * @param steps: # of steps into the future to predict
 * @param name: path of the file where we save the error matrices
 * */
struct predictor_perfect_t *predictor_perfect_create(char const *loc);
void predictor_perfect_save(struct predictor_t *);
void predictor_perfect_free(struct predictor_t *);

struct predictor_perfect_t *
  predictor_perfect_load(struct traffic_matrix_trace_t *);

#endif // _PREDICTORS_EWMA_H_
