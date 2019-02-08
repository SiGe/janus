#ifndef _PREDICTORS_SCALED_EWMA_H_
#define _PREDICTORS_SCALED_EWMA_H_

#include "predictor.h"

struct predictor_scaled_ewma_iterator_t {
  struct predictor_iterator_t;

  // Book keeping
  trace_time_t pos;
  uint32_t sample;
};

struct predictor_scaled_ewma_t {
  struct predictor_t;

  // Prediction trace
  struct traffic_matrix_trace_t **pred_traces;

  // Error trace is the trace after prediction
  struct traffic_matrix_trace_t **error_traces;

  // Real trace upon which the model is built
  struct traffic_matrix_trace_t *real_trace;

  const char *prefix;

  // Exponential coeff of backing off
  bw_t exp_coeff;

  // How many steps does the exp back off run
  uint16_t steps;
};

void predictor_scaled_ewma_build
  (struct predictor_t *, struct traffic_matrix_trace_t *);

/*
 * @param exp_coeff: Exponential smoothing factor
 * @param steps: # of steps into the future to predict
 * @param name: path of the file where we save the error matrices
 * */
struct predictor_scaled_ewma_t *predictor_scaled_ewma_create(
    bw_t coeff, uint16_t steps, const char *loc);

struct predictor_iterator_t* predictor_scaled_ewma_predict(
    struct predictor_t *, struct traffic_matrix_t const*, trace_time_t, trace_time_t);

void predictor_scaled_ewma_save(struct predictor_t *);
void predictor_scaled_ewma_free(struct predictor_t *);

struct predictor_scaled_ewma_t *
  predictor_scaled_ewma_load(char const *, char const *, int, int);

#endif // _PREDICTORS_DOUBLE_EWMA_H_
