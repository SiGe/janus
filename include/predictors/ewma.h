#ifndef _PREDICTORS_EWMA_H_
#define _PREDICTORS_EWMA_H_

#include "predictor.h"

struct predictor_ewma_iterator_t {
  struct predictor_iterator_t;

  // Book keeping
  trace_time_t pos;
  uint32_t sample;
};

struct predictor_ewma_t {
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

void predictor_ewma_build
  (struct predictor_t *, struct traffic_matrix_trace_t *);

struct predictor_ewma_t *predictor_ewma_create(
    bw_t exp_coeff, uint16_t, const char *);

struct predictor_iterator_t* predictor_ewma_predict(
    struct predictor_t *, struct traffic_matrix_t const*, trace_time_t, trace_time_t);

void predictor_ewma_save(struct predictor_t *);
void predictor_ewma_free(struct predictor_t *);

struct predictor_ewma_t *
  predictor_ewma_load(char const *);

#endif // _PREDICTORS_EWMA_H_
