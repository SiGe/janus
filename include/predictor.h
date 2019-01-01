#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "traffic.h"

struct predictor_t;

typedef void (*predictor_free)
  (struct predictor_t *);

typedef void (*predictor_build)
  (struct predictor_t *, struct traffic_matrix_trace_t *);

typedef struct predictor_iterator_t* (*predictor_predict)
  (struct predictor_t *, struct traffic_matrix_t const*, trace_time_t, trace_time_t);

struct predictor_t {
  predictor_free    free;
  predictor_build   build;
  predictor_predict predict;
};

struct predictor_iterator_t {
  struct predictor_t *predictor;
  struct traffic_matrix_t const *tm;
  trace_time_t s, e;

  void (*begin)(
      struct predictor_iterator_t *);

  int (*next)(
      struct predictor_iterator_t *);

  int (*end)(
      struct predictor_iterator_t *);

  void (*free)(
      struct predictor_iterator_t *);

  struct traffic_matrix_t * (*cur)(
      struct predictor_iterator_t *);
};


#endif //_PREDICTOR_H_
