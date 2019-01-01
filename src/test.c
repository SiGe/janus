#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algo/maxmin.h"
#include "networks/jupiter.h"
#include "predictors/ewma.h"
#include "util/log.h"

#include "traffic.h"

#define RUN_COUNT 10
#define abs(p) ((p) < 0 ? -(p) : (p))

void traffic_matrix_random(
    struct traffic_matrix_t **ret, uint32_t num_tors, bw_t bw, float density) {

  uint32_t num_flows =  num_tors * num_tors;
  size_t size = sizeof(struct traffic_matrix_t) + \
               sizeof(struct pair_bw_t) * num_flows;
  struct traffic_matrix_t *tm = malloc(size);
  memset(tm, 0, size);
  (tm)->num_pairs = num_flows;

  //(*tm)->bws = (struct pair_bw_t *)(((char*) *tm) + sizeof(struct traffic_matrix_t));
  struct pair_bw_t *pair = (tm)->bws;

  for (uint32_t s = 0; s < num_tors; ++s) {
    for (uint32_t d = 0; d < num_tors; ++d) {
      if ((float)rand()/(float)RAND_MAX < density && s != d) {
        pair->bw = ((float)rand() / (float)RAND_MAX) * bw;
      } else {
        pair->bw = 0;
      }
      pair++;
    }
  }

  *ret = tm;
}

void traffic_matrix_fixed(struct traffic_matrix_t **ret, uint32_t num_tors, bw_t bw) {
  uint32_t num_flows =  num_tors * num_tors;
  size_t size = sizeof(struct traffic_matrix_t) + \
               sizeof(struct pair_bw_t) * num_flows;
  struct traffic_matrix_t *tm = malloc(size);
  memset(tm, 0, size);
  (tm)->num_pairs = num_flows;

  struct pair_bw_t *pair = (tm)->bws;

  for (uint32_t s = 0; s < num_tors; ++s) {
    for (uint32_t d = 0; d < num_tors; ++d) {
      pair->bw = bw;
      pair++;
    }
  }

  *ret = tm;
}

void traffic_matrix_example(struct traffic_matrix_t **tm, bw_t bw) {
  // Each dude can send traffic to 3 other dudes.
  uint32_t __attribute__((unused)) num_pairs = 4 * 3;
}

void network_stats(struct dataplane_t *network) {
  struct flow_t *flow = 0;
  for (int i = 0; i < network->num_flows; ++i) {
    flow = &network->flows[i];
    printf("Flow %d - bandwidth/demand: (%.2f/%.2f) = %.1f%%\n", 
        i, flow->bw, flow->demand, flow->bw/flow->demand * 100);
  }
}

void test_jupiter_cluster(void) {
  printf("Testing is initialized.");
  uint32_t num_pods = 2; //32
  uint32_t num_aggs = 2;  //24
  uint32_t num_tors = 4; //48
  uint32_t num_cores = 4; //96

  //num_pods = 32;
  //num_aggs = 24;
  //num_tors = 48;
  //num_cores = 96;

  bw_t bw = 10;

  struct network_t *net = jupiter_network_create(
      num_cores, num_pods, num_aggs, num_tors, bw);

  struct traffic_matrix_t *tm = 0;

  struct dataplane_t dp = {0};
  traffic_matrix_random(&tm, num_tors * num_pods, bw, 0.2);
  jupiter_set_traffic(net, tm);

  for (uint32_t i = 0; i < num_aggs-1; ++i) {
    jupiter_drain_switch(net, 
        jupiter_get_agg(net, 0, i));
  }

  for (uint32_t i = 0; i < RUN_COUNT; ++i) {
    jupiter_get_dataplane(net, (&dp));
    maxmin(&dp);
  }
  dataplane_init(&dp);
  jupiter_network_free((struct jupiter_network_t*)net);
  free(tm);
}

void is_tm_equal(struct traffic_matrix_t *t1, struct traffic_matrix_t *t2) {
  assert(t1->num_pairs == t2->num_pairs);

  struct pair_bw_t *p1, *p2;
  p1 = t1->bws;
  p2 = t2->bws;

  for (uint32_t i = 0; i < t1->num_pairs; ++i) {
    assert(p1->bw == p2->bw);
    p1++; p2++;
  }
}

void test_tm_read_load(void) {
  printf("Testing is initialized.");
  uint32_t num_pods = 2; //32
  uint32_t num_tors = 2; //48

  num_pods = 32;
  num_tors = 48;
  bw_t bw = 10;

  const uint32_t replication = 28;//00;

  struct traffic_matrix_t *tm = 0;
  traffic_matrix_random(&tm, num_tors * num_pods, bw, 0.1);

  FILE *f = fopen("tm_file.tm", "wb+");
  fseek(f, 0, SEEK_SET);
  for (uint32_t i = 0; i < replication; ++i) {
    traffic_matrix_save(tm, f);
  }
  fclose(f);

  f = fopen("tm_file.tm", "rb");
  fseek(f, 0, SEEK_SET);
  for (uint32_t i = 0; i < replication; ++i) {
    struct traffic_matrix_t* tm2 = traffic_matrix_load(f);
    is_tm_equal(tm, tm2);
    traffic_matrix_free(tm2);
  }
  fclose(f);

  traffic_matrix_free(tm);
}

struct traffic_matrix_trace_t *gen_sample_trace(uint16_t caches, const char *fname, uint16_t num_traces) {
  struct traffic_matrix_trace_t *ret = traffic_matrix_trace_create(caches, 1000, fname);
  uint32_t num_tors = 48, num_pods = 32;
  bw_t bw = 10;

  for (uint32_t i = 0; i < num_traces; ++i) {
    struct traffic_matrix_t *tm = 0;
    traffic_matrix_random(&tm, num_tors * num_pods, bw, 0.1);
    traffic_matrix_trace_add(ret, tm, i * 100);
    traffic_matrix_free(tm);
  }

  traffic_matrix_trace_save(ret);

  return ret;
}

struct traffic_matrix_trace_t *load_sample_trace(uint16_t caches, const char *fname) {
  struct traffic_matrix_trace_t *ret = traffic_matrix_trace_load(caches, fname);
  return ret;
}

void test_tm_trace(void) {
  uint64_t num_indices = 200;
  struct traffic_matrix_trace_t *trace1 = gen_sample_trace(101, "sample-trace", num_indices);
  struct traffic_matrix_trace_t *trace2 = load_sample_trace(101, "sample-trace");
  struct traffic_matrix_t *tm1 = 0, *tm2 = 0;

  for (uint32_t j = 0; j < 20; ++j) {
    for (uint32_t i = 0; i < num_indices; ++i) {
      traffic_matrix_trace_get(trace1, i * 100, &tm1);
      traffic_matrix_trace_get(trace2, i * 100, &tm2);

      if (tm1 == 0 || tm2 == 0) {
        panic("One of %p or %p is zero at i = %d", tm1, tm2, i * 100);
      }
      is_tm_equal(tm1, tm2);

      traffic_matrix_free(tm1);
      traffic_matrix_free(tm2);
    }
  }

  traffic_matrix_trace_free(trace1);
  traffic_matrix_trace_free(trace2);
}

void test_predictor(void) {
  // So here's how the code should look:
  //
  // model     = model_build_predictor(trace);
  // mop       = mop_create();
  // 
  // network   = network_at(TIME)
  //
  // network->save()
  // while (mop) {
  //   plans = plan_enumerator(network, mop);
  //   condition_long  = slo_condition();
  //   condition_short = slo_condition();
  //
  //   for_each (plan in plans) {
  //      network->apply(mop);
  //   
  //      // Executing this actually takes a long long time.  So we have to
  //      // Preprocess and cache this, somehow.
  //      long_term->simulate(plan[1:], condition_long, predictor, network)
  //   
  //      // This is faster to simulate since it's only for one step. 
  //      // Short term planner relies on predictions for the next N (200?) samples
  //      // of data-point.
  //      short_term->simulate(plan[0], condition_short, predictor, network)
  //
  //      impact_total = condition_long->impact + condition_short->impact;
  //   
  //      if impact_total < threshold {
  //        mop = mop_diff(mop, first_subplan(plan))
  //      }
  //      network->restore();
  //
  //      // Set the current traffic for the network
  //      network->set_traffic(XX);
  //      network->apply(plan[0]);
  //   }
  // }
}

#define EWMA_COEFF 0.8
#define EWMA_BW    1
#define EWMA_STEPS 10
#define EWMA_TRACE_LENGTH 21
#define EWMA_PRED 6

#define EPS 1e-3

struct traffic_matrix_trace_t *ewma_trace(bw_t bw) {
  struct traffic_matrix_trace_t *trace =
    traffic_matrix_trace_create(100, 100, "ewma");

  struct traffic_matrix_t *tm = 0;

  for (uint32_t i = 0; i < EWMA_TRACE_LENGTH; ++i) {
    traffic_matrix_fixed(&tm, 3, bw);
    traffic_matrix_trace_add(trace, tm, i);
    traffic_matrix_free(tm);
  }

  return trace;
}

struct _ewma_metadata {
  bw_t bw[EWMA_STEPS];
  bw_t coeff;
  uint32_t steps;
  bw_t (*traffic_at) (trace_time_t);
};

int _ewma_error_trace_test(
    struct traffic_matrix_t *tm, trace_time_t time, void *metadata) {
  struct pair_bw_t *pair = tm->bws;

  struct _ewma_metadata *ewma = (struct _ewma_metadata*)metadata;
  bw_t *bw = (bw_t*)(ewma->bw);
  bw_t traffic = ewma->traffic_at(time);

  if (time > EWMA_TRACE_LENGTH) {
    traffic_matrix_free(tm);
    return 1;
  }

  if (time < ewma->steps) {
    traffic_matrix_free(tm);
    assert(pair->bw == traffic);
    return 1;
  }

  // We build our ewma prediction from traffic that flowed ewma->steps before
  bw[0] = bw[0] * (1-ewma->coeff) + 
    ewma->traffic_at(time - ewma->steps) * (ewma->coeff);

  for (int i = 1; i < ewma->steps; ++i) {
    // And we repeat the computation steps times to get our prediction
    bw[i] = bw[i] * (1-ewma->coeff) +
      bw[i-1]  * (ewma->coeff);
  }

  for (int i = 0; i < tm->num_pairs; ++i) {
    bw_t diff = pair->bw - (traffic - bw[ewma->steps-1]);
    info("@%d [step: %d] - %f vs. %f", time, ewma->steps, pair->bw, traffic - bw[ewma->steps-1]);
    diff = abs(diff);
    assert(diff < EPS);
    pair++;
  }

  traffic_matrix_free(tm);
  return 1;
}

int _ewma_pred_trace_test(
    struct traffic_matrix_t *tm, trace_time_t time, void *metadata) {
  struct pair_bw_t *pair = tm->bws;

  struct _ewma_metadata *ewma = (struct _ewma_metadata*)metadata;
  bw_t *bw = (bw_t*)(ewma->bw);

  if (time > EWMA_TRACE_LENGTH) {
    traffic_matrix_free(tm);
    return 1;
  }

  if (time < ewma->steps) {
    assert(pair->bw == 0);
    traffic_matrix_free(tm);
    return 1;
  }

  // We build our ewma prediction from traffic that flowed ewma->steps before
  bw[0] = bw[0] * (1-ewma->coeff) + 
    ewma->traffic_at(time - ewma->steps) * (ewma->coeff);

  for (int i = 1; i < ewma->steps; ++i) {
    // And we repeat the computation steps times to get our prediction
    bw[i] = bw[i] * (1-ewma->coeff) +
      bw[i-1]  * (ewma->coeff);
  }

  for (int i = 0; i < tm->num_pairs; ++i) {
    bw_t diff = pair->bw - bw[ewma->steps-1];
    //info("@%d [step: %d] - %f vs. %f", time, ewma->steps, pair->bw, bw[ewma->steps-1]);
    diff = abs(diff);
    assert(diff < EPS);
    pair++;
  }

  traffic_matrix_free(tm);
  return 1;
}

bw_t _const_traffic(trace_time_t _) {
  return 1;
}

void test_ewma(void) {
  const bw_t base_bw = EWMA_BW;
  const bw_t coeff = EWMA_COEFF;
  const uint32_t steps = EWMA_STEPS;

  struct predictor_ewma_t *ewma = predictor_ewma_create(coeff, steps, "ewma");
  struct traffic_matrix_trace_t *trace = ewma_trace(base_bw);
  ewma->build((struct predictor_t *)ewma, trace);

  for (uint32_t i = 0; i < steps; ++i) {
    struct _ewma_metadata mt = {
      .steps = i,
      .bw = {0},
      .coeff = EWMA_COEFF,
      .traffic_at = _const_traffic,
    };
    traffic_matrix_trace_for_each(
        ewma->pred_traces[i], _ewma_pred_trace_test, &mt);

    struct _ewma_metadata emt = {
      .steps = i,
      .bw = {0},
      .coeff = EWMA_COEFF,
      .traffic_at = _const_traffic,
    };
    traffic_matrix_trace_for_each(
        ewma->error_traces[i], _ewma_error_trace_test, &emt);
  }

  struct traffic_matrix_t *tm = 0;
  traffic_matrix_trace_get(trace, EWMA_PRED, &tm);
  struct predictor_iterator_t *iter = ewma->predict(
      (struct predictor_t *)ewma, tm, EWMA_PRED, EWMA_PRED + EWMA_STEPS-1);

  for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
    struct traffic_matrix_t *t1 = iter->cur(iter);
    warn("Bandwidth of the first element is: %f", t1->bws[0].bw);
    // TODO: add checks here.
    warn("TODO: Add checks here ... ");
    traffic_matrix_free(t1);
  }
  iter->free(iter);
  traffic_matrix_free(tm);
  ewma->free((struct predictor_t *)ewma);
  traffic_matrix_trace_free(trace);
}

int main(int argc, char **argv) {
  //test_jupiter_cluster();
  //test_tm_read_load();
  //test_tm_trace();
  test_ewma();
  return 0;
}
