#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/common.h"
#include "util/log.h"
#include "predictors/ewma.h"

#define TO_E(p) struct predictor_ewma_t *pe = (struct predictor_ewma_t *)(p)
#define INDEX(p, j) (p)%(j)
#define EWMA(n, o, coeff) (((n) * (coeff)) + (o) * (1 - (coeff)))

#define EWMA_DEFAULT_INDEX_SIZE 4000
#define EWMA_DEFAULT_CACHE_SIZE 4000

#define PRED_SUFFIX ".pred."
#define ERROR_SUFFIX ".error."

struct predictor_ewma_t *predictor_ewma_create(
    bw_t exp_coeff, uint16_t steps, char const *name) {
  if (steps > EWMA_MAX_TM_STRIDE)
    panic("Too many steps for prediction using this EWMA" 
        "predictor.  Update EWMA_MAX_TM_STRIDE in config.h");

  struct predictor_ewma_t *ewma = malloc(sizeof(struct predictor_ewma_t));

  char error_name[PATH_MAX+1] = {0};
  strncat(error_name, name, PATH_MAX);
  strncat(error_name, ERROR_SUFFIX, PATH_MAX);

  ewma->exp_coeff   = exp_coeff;
  ewma->error_traces = malloc(sizeof(struct traffix_matrix_trace *) * steps);
  ewma->pred_traces  = malloc(sizeof(struct traffix_matrix_trace *) * steps);

  for (uint16_t i = 0; i < steps; ++i) {
    char pred_name[PATH_MAX+1] = {0};
    char num[INT_MAX_LEN];
    snprintf(num, INT_MAX_LEN, "%d", i);
    strncat(pred_name, name, PATH_MAX);
    strncat(pred_name, PRED_SUFFIX, PATH_MAX);
    strncat(pred_name, num, PATH_MAX);
    ewma->pred_traces[i] = traffic_matrix_trace_create(
        EWMA_DEFAULT_CACHE_SIZE, EWMA_DEFAULT_INDEX_SIZE, pred_name);
  }

  for (uint16_t i = 0; i < steps; ++i) {
    char error_name[PATH_MAX+1] = {0};
    char num[INT_MAX_LEN];
    snprintf(num, INT_MAX_LEN, "%d", i);
    strncat(error_name, name, PATH_MAX);
    strncat(error_name, ERROR_SUFFIX, PATH_MAX);
    strncat(error_name, num, PATH_MAX);
    ewma->error_traces[i] = traffic_matrix_trace_create(
        EWMA_DEFAULT_CACHE_SIZE, EWMA_DEFAULT_INDEX_SIZE, error_name);
  }

  ewma->real_trace  = 0;
  ewma->predict     = predictor_ewma_predict;
  ewma->build       = predictor_ewma_build;
  ewma->free        = predictor_ewma_free;
  ewma->steps       = steps;

  return ewma;
}

#define TO_PI(p) struct predictor_ewma_iterator_t *iter =\
    (struct predictor_ewma_iterator_t *)(p)
static void _pe_begin(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);

  iter->pos = iter->s + 1;
}

static int _pe_next(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);

  if (iter->pos <=  iter->e) {
    iter->pos++;
    return 1;
  }

  return 0;
}

static int _pe_end(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);

  return (iter->pos > iter->e);
}

static struct traffic_matrix_t * _pe_cur(
    struct predictor_iterator_t *pi) {
  TO_PI(pi);
  TO_E(iter->predictor);

  trace_time_t step = iter->pos - iter->s;

  struct traffic_matrix_t *err = 0, *out = 0;
  traffic_matrix_trace_get(
      pe->error_traces[step],
      iter->s, &err);

  out = traffic_matrix_add(iter->tm, err);
  traffic_matrix_free(err);
  return out;
}

static void _pe_free(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);
  free(iter);
}



/* Returns an array of traffic matrices */
struct predictor_iterator_t *predictor_ewma_predict(
    struct predictor_t *pre, struct traffic_matrix_t const *tm,
    trace_time_t s, trace_time_t e) {

  TO_E(pre);
  if (e - s >= pe->steps) {
    panic("Cannot predict that far into the future: %d, %d (num steps: %d)", e, s, pe->steps);
    return 0;
  }

  struct predictor_ewma_iterator_t *iter = malloc(
      sizeof(struct predictor_ewma_iterator_t));


  iter->begin = _pe_begin;
  iter->end = _pe_end;
  iter->free = _pe_free;
  iter->cur = _pe_cur;
  iter->next = _pe_next;

  iter->s = s;
  iter->e = e;
  iter->predictor = pre;
  iter->tm = tm;

  return (struct predictor_iterator_t *)iter;
}


struct _ewma_rotating_predictor_t {
  struct traffic_matrix_t *pred[EWMA_MAX_TM_STRIDE];
  struct traffic_matrix_t *prev[EWMA_MAX_TM_STRIDE];
  struct traffic_matrix_t *error[EWMA_MAX_TM_STRIDE];
  struct traffic_matrix_t *real[EWMA_MAX_TM_STRIDE];
  uint16_t index;
  uint16_t size;
  struct predictor_ewma_t *ewma;
  trace_time_t keys[EWMA_MAX_TM_STRIDE];
};

int _ewma_predictor_rotating_func(
    struct traffic_matrix_t *future_tm, 
    trace_time_t time,
    void *metadata) {

  struct _ewma_rotating_predictor_t *setting = 
    (struct _ewma_rotating_predictor_t *)metadata;
  struct predictor_ewma_t *pe = setting->ewma;

  // Allocate enough TMs for predictions
  uint32_t index = INDEX(setting->index, setting->size);
  setting->keys[index] = time;

  struct traffic_matrix_t *tm = setting->real[index];

  // Build the prediction matrices
  /*
  for (uint32_t i = 0; i < setting->size; ++i) {
    setting->pred[i] = malloc(
      sizeof(struct traffic_matrix_t) +
      sizeof(struct pair_bw_t) * tm->num_pairs);
    setting->pred[i]->num_pairs = tm->num_pairs;
  }

  // Build space for error matrices
  for (uint32_t i = 0; i < setting->size; ++i) {
    setting->error[i] = malloc(
      sizeof(struct traffic_matrix_t) +
      sizeof(struct pair_bw_t) * tm->num_pairs);
    setting->error[i]->num_pairs = tm->num_pairs;
  }
  */

  // Set the pointers
  // Used to compute the errors
  struct pair_bw_t *real_stride[EWMA_MAX_TM_STRIDE] = {0};
  // Keeps track of new TM's values
  struct pair_bw_t *pred_stride[EWMA_MAX_TM_STRIDE] = {0};
  // Keeps track of error TM's values
  struct pair_bw_t *error_stride[EWMA_MAX_TM_STRIDE] = {0};
  // Keeps track of previously calculated TMs
  struct pair_bw_t *prev_stride[EWMA_MAX_TM_STRIDE] = {0};

  for (uint32_t i = 0; i < setting->size; ++i) {
    // Normalize the rotating pointers ...
    real_stride[i]  = setting->real[i]->bws;
    prev_stride[i]  = setting->prev[i]->bws;
    pred_stride[i]  = setting->pred[i]->bws;
    error_stride[i] = setting->error[i]->bws;
  }

  bw_t coeff = pe->exp_coeff;
  for (uint32_t _ = 0; _ < tm->num_pairs; ++_) {
    pred_stride[0]->bw = real_stride[index]->bw;

    for (uint32_t i = 1; i < setting->size; ++i) {
      pred_stride[i]->bw = EWMA(
          pred_stride[i-1]->bw, 
          prev_stride[i]->bw, coeff);

      // Error is from the previous run
      error_stride[i]->bw =
        real_stride[INDEX(i+index-1, setting->size)]->bw - pred_stride[i]->bw;

      /*
      info("@%d PREDICTION FOR %d is %f (error=%f, real=%f)", 
          setting->keys[INDEX(index+i, setting->size)], i, 
          pred_stride[i]->bw, error_stride[i]->bw,
          real_stride[INDEX(i+index-1, setting->size)]->bw);
      */
    }

    for (uint32_t i = 1; i < setting->size; ++i) {
      prev_stride[i]->bw = pred_stride[i]->bw;
    }

    for (uint32_t i = 0; i < setting->size; ++i) {
      pred_stride[i]++;
      error_stride[i]++;
      real_stride[i]++;
      prev_stride[i]++;
    }
  }
  
  for (uint32_t i = 1; i < setting->size; ++i) {
    // We have padded the first i pred with i zero matrices
    // for the first i key.  The ith prediction is time + ith
    // key
    traffic_matrix_trace_add(pe->pred_traces[i], 
        setting->pred[i], setting->keys[INDEX(index+i, setting->size)]);

    //info("Adding key for %d's error matrix (time=%d)", i, setting->keys[INDEX(index+i, setting->size)]);
    traffic_matrix_trace_add(pe->error_traces[i], 
        setting->error[i], setting->keys[INDEX(index+i, setting->size)]);
  }

  // Save the TM for later comparisons
  if (setting->real[index]) {
    traffic_matrix_free(setting->real[index]);
  }

  setting->real[index] = future_tm;
  setting->index++;

  /* We just need to free the real traffic martices and the space allocated for
   * error/pred/prev traces at the end */
  return 1;
}

// 1 + p + p^2 + ... p^n = (1 + p^(n+1))/(1 + p)
// 
void predictor_ewma_build(
    struct predictor_t *p, 
    struct traffic_matrix_trace_t *trace) {
  TO_E(p);
  pe->real_trace = trace;
  if (trace->num_indices == 0)
    panic("Cannot build an EWMA model with no entry in the trace");

  // TODO: we need to get the num pairs---there should be a better way of doing this.
  //  Omid - 12/26/2018
  struct traffic_matrix_t *tm = 0;
  trace_time_t time = 0;
  if (traffic_matrix_trace_get_nth_key(trace, 0, &time) != SUCCESS)
    panic("Trace is empty.");
  traffic_matrix_trace_get(trace, time, &tm);
  uint32_t num_pairs = tm->num_pairs;

  struct _ewma_rotating_predictor_t rpt = {
    .pred = {0},
    .prev = {0},
    .error= {0},
    .real = {0},
    .index = 0,
    .size = pe->steps,
    .ewma = pe,
    .keys = {0},
  };

  for (uint32_t i = 0; i < rpt.size; ++i) {
    if (traffic_matrix_trace_get_nth_key(trace, i, &rpt.keys[i]) != SUCCESS)
      panic("Couldn't find enough data in the trace.");
  }

  /* Prepare the rotating predictor */
  struct traffic_matrix_t *zero_tm  = traffic_matrix_zero(num_pairs);
  for (uint32_t i = 0; i < rpt.size; ++i) {
    /* Set the error, prev, pred, and real traffic matrices */
    rpt.error[i] = traffic_matrix_zero(num_pairs);
    rpt.prev[i]  = traffic_matrix_zero(num_pairs);
    rpt.pred[i]  = traffic_matrix_zero(num_pairs);

    /* Add i zero traffic matrices to each prediction trace */
    for (uint32_t j = 0; j < i; ++j) {
      /* TODO: Are we using the rpt.keys? */
      traffic_matrix_trace_get_nth_key(trace, i, &rpt.keys[i]);

      /* Setup the zero matrix */
      trace_time_t key = 0;
      traffic_matrix_trace_get_nth_key(trace, j, &key);
      traffic_matrix_trace_add(pe->pred_traces[i], zero_tm, key);
    }

    /* Setup the initial error matrix values */
    for (uint32_t j = 0; j < i; ++j) {
      trace_time_t key = 0;
      traffic_matrix_trace_get_nth_key(trace, j, &key);
      struct traffic_matrix_t *tm = 0;
      traffic_matrix_trace_get(trace, key, &tm);
      traffic_matrix_trace_add(pe->error_traces[i], tm, key);
      traffic_matrix_free(tm);
    }
  }
  traffic_matrix_free(tm);

  for (uint64_t i = 0; i < pe->steps; ++i) {
    struct traffic_matrix_t *tm = 0;
    struct traffic_matrix_trace_index_t *index = &trace->indices[i];
    traffic_matrix_trace_get(trace, index->time, &tm);
    rpt.real[i] = tm;
  }

  trace_time_t last = 0;
  /* Execute the ewma_predictor_rotating func */
  uint32_t end = 0;
  if (pe->steps < trace->num_indices)
    end = trace->num_indices - pe->steps;
  for (uint32_t i = 0; i < end; ++i) {
    struct traffic_matrix_trace_index_t *index = &trace->indices[i + pe->steps];
    traffic_matrix_trace_get(trace, index->time, &tm);
    last = index->time;

    if (!tm)
      panic("TM is null.  Expected a TM for key: %d", index->time);

    if (!_ewma_predictor_rotating_func(tm, index->time, &rpt))
      break;
  }

  uint32_t start = 0;
  if (pe->steps < trace->num_indices)
    start = trace->num_indices - pe->steps;

  for (uint64_t i = start, j = 0; i < trace->num_indices; ++i, ++j) {
    if (!_ewma_predictor_rotating_func(
          traffic_matrix_zero(num_pairs), last + j + 1, &rpt))
      break;
  }

  for (uint32_t i = 0; i < rpt.size; ++i) {
    traffic_matrix_free(rpt.error[i]);
    traffic_matrix_free(rpt.prev[i]);
    traffic_matrix_free(rpt.pred[i]);
    traffic_matrix_free(rpt.real[i]);
  }

  traffic_matrix_free(zero_tm);
  predictor_ewma_save(p);

  //TODO: REMOVE THIS
  /*
  struct traffic_matrix_trace_t *pri = pe->error_traces[pe->steps-1];
  for (uint32_t i = 0; i < pri->num_indices; ++i) {
    trace_time_t key = 0;
    traffic_matrix_trace_get_nth_key(pri, i, &key);
    traffic_matrix_trace_get(pri, key, &tm);
    traffic_matrix_free(tm);
  }
  */
}

void predictor_ewma_save(struct predictor_t *predictor) {
  TO_E(predictor);
  for (uint32_t i = 0; i < pe->steps; ++i) {
    traffic_matrix_trace_save(pe->pred_traces[i]);
    traffic_matrix_trace_save(pe->error_traces[i]);
  }
}

void predictor_ewma_free(struct predictor_t *predictor) {
  TO_E(predictor);

  for (uint32_t i = 0; i < pe->steps; ++i) {
    traffic_matrix_trace_free(pe->pred_traces[i]);
    traffic_matrix_trace_free(pe->error_traces[i]);
  }
  free(pe->error_traces);
  free(pe->pred_traces);
  free(pe);
}


static void predictor_ewma_free_error_only(struct predictor_t *predictor) {
  TO_E(predictor);

  for (uint32_t i = 0; i < pe->steps; ++i) {
    traffic_matrix_trace_free(pe->error_traces[i]);
  }
  free(pe->error_traces);
  free(pe);
}

void predictor_ewma_build_panic(
    struct predictor_t *p, 
    struct traffic_matrix_trace_t *trace) {
  (void)p;
  (void)trace;
  panic("Cannot build an ewma predictor that is loaded from file.");
}

struct predictor_ewma_t *predictor_ewma_load(char const *dir, char const *fname, int steps, int cache_size) {
  if (!dir_exists(dir))
    return 0;

  // Load the error matrix
  // TODO: Not a good idea but unfortunately this object should be "different"
  // than the typical EWMA thing Probably a better idea not to have a "build"
  // function in the predictor.  It's sort of meaningless to begin with.
  //
  // - Omid 1/23/2019
  struct predictor_ewma_t *ewma = malloc(sizeof(struct predictor_ewma_t));
  ewma->error_traces = malloc(sizeof(struct traffix_matrix_trace *) * steps);
  ewma->build = predictor_ewma_build_panic;
  ewma->predict = predictor_ewma_predict;
  ewma->free = predictor_ewma_free_error_only;
  ewma->steps = steps;

  for (uint32_t i = 0; i < steps; ++i) {
    char fpath[PATH_MAX] = {0};
    sprintf(fpath, "%s" PATH_SEPARATOR "%s" ERROR_SUFFIX "%d", dir, fname, i);
    ewma->error_traces[i] = traffic_matrix_trace_load(cache_size, fpath);
  }

  return ewma;
}
