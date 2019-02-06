#ifndef _PREDICTOR_H_
#define _PREDICTOR_H_

#include "traffic.h"

struct predictor_t;

typedef void (*predictor_free)
  (struct predictor_t *);

typedef void (*predictor_build)
  (struct predictor_t *, struct traffic_matrix_trace_t *);

// TODO: the predict function expects that the predictor already knows about the
// real traffic matrix trace that is being replayed.  For this to be true, the
// predictor needs to have received the real traffic matrix at some point.
//
// Right now, EWMA and Perfect are getting notified of the full traffic trace
// history when they are _loaded.
//
// Maybe think about changing the interface later?
//
// - Omid 1/31/2019
typedef struct predictor_iterator_t *(*predictor_predict)
  (struct predictor_t *, trace_time_t start, trace_time_t end);

struct predictor_iterator_t {
  struct predictor_t *predictor;
  trace_time_t        s, e;

  void (*begin)(
      struct predictor_iterator_t *);

  int (*next)(
      struct predictor_iterator_t *);

  int (*end)(
      struct predictor_iterator_t *);

  int (*length)(
      struct predictor_iterator_t *);

  void (*free)(
      struct predictor_iterator_t *);

  struct traffic_matrix_trace_iter_t* (*cur)(
      struct predictor_iterator_t *);
};

struct predictor_t {
  predictor_free    free;
  predictor_build   build;
  predictor_predict predict;
};

#endif //_PREDICTOR_H_
