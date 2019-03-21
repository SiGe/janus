#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/common.h"
#include "util/log.h"
#include "predictors/perfect.h"

#define TO_P(p) struct predictor_perfect_t *pp =\
    (struct predictor_perfect_t *)(p)
#define TO_PI(p) struct predictor_perfect_iterator_t *iter =\
    (struct predictor_perfect_iterator_t *)(p)

static void _pe_begin(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);

  iter->done = 0;
}

static int _pe_next(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);
  iter->done = 1;

  return (iter->done == 1);
}

static int _pe_end(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);

  return (iter->done == 1);
}

static struct traffic_matrix_trace_iter_t * _pe_cur(
    struct predictor_iterator_t *pi) {
  TO_PI(pi);
  TO_P(iter->predictor);
  struct traffic_matrix_trace_iter_t *titer = pp->trace->iter(pp->trace);
  trace_iterator_set_range(titer, iter->s+1, iter->e+1);
  titer->go_to(titer, iter->s);
  return titer;
}

static void _pe_free(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);
  free(iter);
}

static unsigned _pe_length(struct predictor_iterator_t *pre) {
  return 1;
}

/* Returns an array of traffic matrices */
static struct predictor_iterator_t *
predictor_perfect_predict(struct predictor_t *pre, trace_time_t s, trace_time_t e) {
  struct predictor_perfect_iterator_t *iter = malloc(
      sizeof(struct predictor_perfect_iterator_t));

  iter->begin = _pe_begin;
  iter->end = _pe_end;
  iter->free = _pe_free;
  iter->cur = _pe_cur;
  iter->next = _pe_next;
  iter->length = _pe_length;

  iter->s = s;
  iter->e = e;
  iter->predictor = pre;

  return (struct predictor_iterator_t *)iter;
}

void predictor_perfect_save(struct predictor_t *predictor) {
  (void)predictor;
}

void predictor_perfect_free(struct predictor_t *predictor) {
  TO_P(predictor);
  free(pp);
}

void predictor_perfect_build_panic(
    struct predictor_t *p, 
    struct traffic_matrix_trace_t *trace) {
  (void)p; (void)trace;
  panic_txt("No need to build the perfect predictor.  Just pass the real trace"
      "during _load'ing the predictor.");
}

struct predictor_perfect_t *predictor_perfect_load(struct traffic_matrix_trace_t *real) {
  struct predictor_perfect_t *perfect = malloc(sizeof(struct predictor_perfect_t));
  perfect->trace = real;
  perfect->build = predictor_perfect_build_panic;
  perfect->predict = predictor_perfect_predict;
  perfect->free = predictor_perfect_free;

  return perfect;
}

struct predictor_perfect_t *predictor_perfect_create(char const *name) {
  panic_txt("There is no need to create a perfect predictor. "
            "Just use the _load functions.");
  return 0;
}

