#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/common.h"
#include "util/log.h"
#include "predictors/rotating_ewma.h"

#define TO_E(p) struct predictor_rotating_ewma_t *pe = (struct predictor_rotating_ewma_t *)(p)
#define INDEX(p, j) (p)%(j)
#define EWMA(n, o, coeff) (((n) * (coeff)) + (o) * (1 - (coeff)))

#define EWMA_DEFAULT_INDEX_SIZE 4000
#define EWMA_DEFAULT_CACHE_SIZE 4000

#define PRED_SUFFIX ".pred."
#define ERROR_SUFFIX ".error."

#define TO_PI(p) struct predictor_rotating_ewma_iterator_t *iter =\
    (struct predictor_rotating_ewma_iterator_t *)(p)
static void _pe_begin(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);

  iter->pos = 0;
}

static int _pe_next(
    struct predictor_iterator_t *piter) {
  TO_PI(piter);
  iter->pos++;

  if (iter->pos >= iter->num_samples)
    iter->pos = iter->num_samples;

  return iter->pos == iter->num_samples;
}

static int _pe_end(
    struct predictor_iterator_t *piter) {
  TO_PI(piter);

  return (iter->pos >= iter->num_samples);
}

static struct traffic_matrix_trace_iter_t * _pe_cur(
    struct predictor_iterator_t *pi) {
  TO_PI(pi);
  TO_E(iter->predictor);

  struct traffic_matrix_t **tms = 
    malloc(sizeof(struct traffic_matrix_t *) * pe->steps);

  trace_time_t offset = iter->num_samples / 2;

  for (uint32_t step = 1; step < pe->steps; ++step) {
    struct traffic_matrix_t *tm = 0;
    traffic_matrix_trace_get(
        pe->error_traces[step],
        iter->s + iter->pos - offset, &tm);
    assert(tm != 0);
    assert(iter->tm_now != 0);
    tms[step-1] = traffic_matrix_add(tm, iter->tm_now);
    traffic_matrix_free(tm);
  }

  struct traffic_matrix_trace_iter_t *ret = 
    traffic_matrix_iter_from_tms(tms, pe->steps);
  return ret;
}

static void _pe_free(
    struct predictor_iterator_t *pe) {
  TO_PI(pe);
  traffic_matrix_free(iter->tm_now);
  iter->tm_now = 0;
  free(iter);
}

static int _pe_length(struct predictor_iterator_t *pre) {
  TO_PI(pre);
  return iter->num_samples;
}

/* Returns an array of traffic matrices */
static struct predictor_iterator_t *predictor_rotating_ewma_predict(
    struct predictor_t *pre, trace_time_t now, trace_time_t end) {

  TO_E(pre);
  if (end - now >= pe->steps || end - now < 0) {
    panic("Cannot predict that far into the future. Asking for range [%d, %d] (num steps: %d)", 
        now, end, pe->steps);
    return 0;
  }

  struct predictor_rotating_ewma_iterator_t *iter = malloc(
      sizeof(struct predictor_rotating_ewma_iterator_t));


  iter->begin = _pe_begin;
  iter->end = _pe_end;
  iter->free = _pe_free;
  iter->cur = _pe_cur;
  iter->next = _pe_next;
  iter->length = _pe_length;

  iter->s = now;
  iter->e = end;
  iter->tm_now = 0;
  iter->predictor = pre;
  iter->num_samples = pe->sample;
  traffic_matrix_trace_get(pe->real_trace, iter->s, &iter->tm_now);
  //info("Creating a an iterator ...: %p, %p, %u, %u", iter, iter->tm_now, now, iter->e);

  return (struct predictor_iterator_t *)iter;
}


struct _rotating_ewma_rotating_predictor_t {
  struct traffic_matrix_t *pred[EWMA_MAX_TM_STRIDE];
  struct traffic_matrix_t *prev[EWMA_MAX_TM_STRIDE];
  struct traffic_matrix_t *error[EWMA_MAX_TM_STRIDE];
  struct traffic_matrix_t *real[EWMA_MAX_TM_STRIDE];
  uint16_t index;
  uint16_t size;
  struct predictor_rotating_ewma_t *rotating_ewma;
  trace_time_t keys[EWMA_MAX_TM_STRIDE];
};

int _rotating_ewma_predictor_rotating_func(
    struct traffic_matrix_t *future_tm, 
    trace_time_t time,
    void *metadata) {

  struct _rotating_ewma_rotating_predictor_t *setting = 
    (struct _rotating_ewma_rotating_predictor_t *)metadata;
  struct predictor_rotating_ewma_t *pe = setting->rotating_ewma;

  // Allocate enough TMs for predictions
  uint32_t index = INDEX(setting->index, setting->size);
  setting->keys[index] = time;

  struct traffic_matrix_t *tm = setting->real[index];

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
    // We have padded the first i pred with i zero matrices for the first i key.
    // The ith prediction is time + ith key
    traffic_matrix_trace_add(pe->pred_traces[i], 
        setting->pred[i], setting->keys[INDEX(index+i, setting->size)]);

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
void predictor_rotating_ewma_build(
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

  struct _rotating_ewma_rotating_predictor_t rpt = {
    .pred = {0},
    .prev = {0},
    .error= {0},
    .real = {0},
    .index = 0,
    .size = pe->steps,
    .rotating_ewma = pe,
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
  /* Execute the rotating_ewma_predictor_rotating func */
  uint32_t end = 0;
  if (pe->steps < trace->num_indices)
    end = trace->num_indices - pe->steps;
  for (uint32_t i = 0; i < end; ++i) {
    struct traffic_matrix_trace_index_t *index = &trace->indices[i + pe->steps];
    traffic_matrix_trace_get(trace, index->time, &tm);
    last = index->time;

    if (!tm)
      panic("TM is null.  Expected a TM for key: %d", index->time);

    if (!_rotating_ewma_predictor_rotating_func(tm, index->time, &rpt))
      break;
  }

  uint32_t start = 0;
  if (pe->steps < trace->num_indices)
    start = trace->num_indices - pe->steps;

  for (uint64_t i = start, j = 0; i < trace->num_indices; ++i, ++j) {
    if (!_rotating_ewma_predictor_rotating_func(
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
  predictor_rotating_ewma_save(p);
}

void predictor_rotating_ewma_save(struct predictor_t *predictor) {
  TO_E(predictor);
  for (uint32_t i = 0; i < pe->steps; ++i) {
    traffic_matrix_trace_save(pe->pred_traces[i]);
    traffic_matrix_trace_save(pe->error_traces[i]);
  }
}

void predictor_rotating_ewma_free(struct predictor_t *predictor) {
  TO_E(predictor);

  for (uint32_t i = 0; i < pe->steps; ++i) {
    traffic_matrix_trace_free(pe->pred_traces[i]);
    traffic_matrix_trace_free(pe->error_traces[i]);
  }
  free(pe->error_traces);
  free(pe->pred_traces);
  free(pe);
}


static void predictor_rotating_ewma_free_error_only(struct predictor_t *predictor) {
  TO_E(predictor);

  for (uint32_t i = 0; i < pe->steps; ++i) {
    traffic_matrix_trace_free(pe->error_traces[i]);
  }
  free(pe->error_traces);
  free(pe);
}

static void predictor_rotating_ewma_build_panic(
    struct predictor_t *p, 
    struct traffic_matrix_trace_t *trace) {
  (void)p;
  (void)trace;
  panic("Cannot build an rotating_ewma predictor that is loaded from file.");
}

struct predictor_rotating_ewma_t *predictor_rotating_ewma_load(
    char const *dir, char const *fname, int steps, int cache_size,
    struct traffic_matrix_trace_t *trace) {
  if (!dir_exists(dir))
    return 0;

  // Load the error matrix
  // TODO: Not a good idea but unfortunately this object should behave
  // "differently" than the object returned by _create. Probably a better idea
  // not to have a "build" function in the predictor.  It's sort of meaningless
  // to begin with.
  //
  // - Omid 1/23/2019
  struct predictor_rotating_ewma_t *rotating_ewma = malloc(sizeof(struct predictor_rotating_ewma_t));
  rotating_ewma->error_traces = malloc(sizeof(struct traffix_matrix_trace *) * steps);
  rotating_ewma->build = predictor_rotating_ewma_build_panic;
  rotating_ewma->predict = predictor_rotating_ewma_predict;
  rotating_ewma->free = predictor_rotating_ewma_free_error_only;
  rotating_ewma->steps = steps;
  rotating_ewma->real_trace = trace;
  rotating_ewma->sample = 40;

  for (uint32_t i = 0; i < steps; ++i) {
    char fpath[PATH_MAX] = {0};
    sprintf(fpath, "%s" PATH_SEPARATOR "%s" ERROR_SUFFIX "%d", dir, fname, i);
    rotating_ewma->error_traces[i] = traffic_matrix_trace_load(cache_size, fpath);
  }

  return rotating_ewma;
}

struct predictor_rotating_ewma_t *predictor_rotating_ewma_create(
    bw_t exp_coeff, uint16_t steps, char const *name) {
  if (steps > EWMA_MAX_TM_STRIDE)
    panic("Too many steps for prediction using this EWMA" 
        "predictor.  Update EWMA_MAX_TM_STRIDE in config.h");

  struct predictor_rotating_ewma_t *rotating_ewma = malloc(sizeof(struct predictor_rotating_ewma_t));

  char error_name[PATH_MAX+1] = {0};
  strncat(error_name, name, PATH_MAX);
  strncat(error_name, ERROR_SUFFIX, PATH_MAX);

  rotating_ewma->exp_coeff   = exp_coeff;
  rotating_ewma->error_traces = malloc(sizeof(struct traffix_matrix_trace *) * steps);
  rotating_ewma->pred_traces  = malloc(sizeof(struct traffix_matrix_trace *) * steps);

  for (uint16_t i = 0; i < steps; ++i) {
    char pred_name[PATH_MAX+1] = {0};
    char num[INT_MAX_LEN];
    snprintf(num, INT_MAX_LEN, "%d", i);
    strncat(pred_name, name, PATH_MAX);
    strncat(pred_name, PRED_SUFFIX, PATH_MAX);
    strncat(pred_name, num, PATH_MAX);
    rotating_ewma->pred_traces[i] = traffic_matrix_trace_create(
        EWMA_DEFAULT_CACHE_SIZE, EWMA_DEFAULT_INDEX_SIZE, pred_name);
  }

  for (uint16_t i = 0; i < steps; ++i) {
    char error_name[PATH_MAX+1] = {0};
    char num[INT_MAX_LEN];
    snprintf(num, INT_MAX_LEN, "%d", i);
    strncat(error_name, name, PATH_MAX);
    strncat(error_name, ERROR_SUFFIX, PATH_MAX);
    strncat(error_name, num, PATH_MAX);
    rotating_ewma->error_traces[i] = traffic_matrix_trace_create(
        EWMA_DEFAULT_CACHE_SIZE, EWMA_DEFAULT_INDEX_SIZE, error_name);
  }

  rotating_ewma->real_trace  = 0;
  rotating_ewma->predict     = predictor_rotating_ewma_predict;
  rotating_ewma->build       = predictor_rotating_ewma_build;
  rotating_ewma->free        = predictor_rotating_ewma_free;
  rotating_ewma->steps       = steps;

  return rotating_ewma;
}
