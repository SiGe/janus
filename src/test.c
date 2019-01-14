#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algo/maxmin.h"
#include "networks/jupiter.h"
#include "predictors/ewma.h"
#include "util/group_gen.h"
#include "util/log.h"

#include "plan.h"
#include "traffic.h"

#define RUN_COUNT 10
#define abs(p) ((p) < 0 ? -(p) : (p))
#define TEST(p) {\
  printf("\t > \e[1;33mTESTING\033[0m %s: ", #p);\
  test_##p();\
  printf("\e[1;33mPASSED\033[0m.\n");\
}

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
  uint32_t num_pods = 2; //32
  uint32_t num_tors = 2; //48

  num_pods = 32/4;
  num_tors = 48/4;
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
  uint32_t num_tors = 48/4, num_pods = 32/4;
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

#define NUM_STATES 10
void test_group_state() {
  uint32_t A000041[] = {1, 1, 2, 3, 5, 7, 11, 15, 22, 30, 42, 56, 77, 101, 135,
    176, 231, 297, 385, 490, 627, 792, 1002, 1255, 1575, 1958, 2436, 3010,
    3718, 4565, 5604, 6842, 8349, 10143, 12310, 14883, 17977, 21637, 26015,
    31185, 37338, 44583, 53174, 63261, 75175, 89134, 105558, 124754, 147273,
    173525};

  for (uint32_t i = 1; i < sizeof(A000041)/sizeof(uint32_t); ++i) {
    uint32_t count = 0;
    struct group_iter_t *s = npart_create(i);

    for (s->begin(s); !s->end(s); s->next(s)) {
      for (uint32_t i = 0; i < s->state_length; ++i) {
        //info("%d ", s->state[i]);
      }
      //info("\n");
      count ++;
    }

    //info("Count for %d is %d", i, count);
    assert(count == A000041[i]);

    s->free(s);
  }
}

void test_dual_state() {
  uint32_t test[][11] = {
    {      1,       3,       6,      11,      18,      29,      44,      66,      96,     138,     194},
    {      3,       8,      15,      28,      46,      76,     117,     180,     266,     391,     559},
    {      6,      15,      30,      56,      96,     161,     256,     400,     607,     906,    1324},
    {     11,      28,      56,     108,     188,     322,     521,     830,    1278,    1940,    2875},
    {     18,      46,      96,     188,     338,     588,     974,    1575,    2471,    3803,    5726},
    {     29,      76,     161,     322,     588,    1042,    1751,    2875,    4570,    7127,   10859},
    {     44,     117,     256,     521,     974,    1751,    2997,    4986,    8042,   12692,   19583},
    {     66,     180,     400,     830,    1575,    2875,    4986,    8405,   13714,   21892,   34133},
    {     96,     266,     607,    1278,    2471,    4570,    8042,   13714,   22651,   36534,   57567},
    {    138,     391,     906,    1940,    3803,    7127,   12692,   21892,   36534,   59520,   94663},
    {    194,     559,    1324,    2875,    5726,   10859,   19583,   34133,   57567,   94663,  151957},
  };

  uint32_t sy = sizeof(test[0])/sizeof(uint32_t);
  uint32_t sx = sizeof(test)/(sy * sizeof(uint32_t));

  (void)sy;
  (void)sx;
  sx = 4; sy = 4;


  for (uint32_t i = 1; i <= sx; ++i) {
    for (uint32_t j = 1; j <= sy; ++j) {
      struct group_iter_t *s1 = npart_create(j);
      struct group_iter_t *s2 = npart_create(i);

      struct group_iter_t *s =
        dual_npart_create(s1, s2);

      uint32_t count = 0;
      for (s->begin(s); !s->end(s); s->next(s)) {
          count ++;
          /*
          printf("(%2d, %2d): ", i, j);
          for (uint32_t k = 0; k < s->state_length; ++k) {
            printf("%5d", s->state[k]);
          }
          printf("\n");
          */
      }
      assert(count == test[i-1][j-1] + 1);

      count = 0;
      for (s->begin(s); !s->end(s); s->next(s)) {
          count ++;
          /*
          printf("(%2d, %2d): ", i, j);
          for (uint32_t k = 0; k < s->state_length; ++k) {
            printf("%5d", s->state[k]);
          }
          printf("\n");
          */
      }

      assert(count == test[i-1][j-1] + 1);
      s1->free(s1);
      s2->free(s2);
      s->free(s);
    }
  }
}

#define TRI_SIZE 5
void test_tri_state() {
  // A219727
  uint32_t test[] = {1, 5, 66, 686, 6721, 58616};
  uint32_t tuple[TRI_SIZE] = {0};

  for(uint32_t i = 5; i <= TRI_SIZE; ++i) {
    struct group_iter_t *s1 = npart_create(i);
    struct group_iter_t *s2 = npart_create(i);
    struct group_iter_t *s3 = npart_create(i);

    struct group_iter_t *s12 =
      dual_npart_create(s1, s2);

    struct group_iter_t *s123 =
      dual_npart_create(s12, s3);

    struct group_iter_t *s = s123;

    uint32_t count = 0;
    for (s->begin(s); !s->end(s); s->next(s)) {
      for (uint32_t k = 0; k < s->state_length; ++k) {
        s->to_tuple(s, s->state[k], tuple);
        //printf("[");
        for (uint32_t p = 0; p < s->tuple_size; ++p) {
          //printf("%d ", tuple[p]);
        }
        //printf("], ");
      }
      //printf("\n");

      count ++;
    }
    assert(count == test[i]);

    s1->free(s1);
    s2->free(s2);
    s3->free(s3);
    s12->free(s12);
    s123->free(s123);
  }
}

void test_experiment(void) {
  // Here's how a (jupiter) experiment should look like:
  //
  //
  // (0) Traffic traces:
  //
  //     (1) Test: Traffic trace for a DC of the same size.
  //     (2) Validation: Traffic trace for a DC over a period of a day
  //
  // (1) A list of pods that are "similar"  (so we plan them the same way)
  // (2) Granularity of search for each pod and the core switches
  //    (If you want to understand what granularity you should choose for each
  //     pod take a look at https://oeis.org/A219727
  //     
  //                      K = 3 (2 different pods + 1 core switch)
	//     1,   1,    1,      1,        1,         1,         1,       1, ...
	//     1,   1,    2,      5,       15,        52,       203,     877, ...
	//     1,   2,    9,     66,      712,     10457,    198091, 4659138, ...
	//     1,   3,   31,    686,    27036,   1688360, 154703688, ...
	//     1,   5,  109,   6721,   911838, 231575143, ...                      N = 5 (upgrade in 5 steps)
	//     1,   7,  339,  58616, 26908756, ...
	//     1,  11, 1043, 476781, ...
	//     1,  15, 2998, ...
  //
  //  (3) Two risk functions:
  //      (1) Gets the percentage of ToR pairs satisfied and returns a "cost" value
  //      (2) Gets the length of a plan (i.e., time) and returns a "cost" value
  //  
  //  (4) We use (0.1) and (3.1) to generate a random variable for the risk of
  //      long-term violation for each possible plan subset.
  //
  //  (5) We use (0.1) to generate an EWMA error matrix.
  //
  //  (6) During online validation, we use (5) to predict the traffic of the
  //  next subset and use (3.1) to translate it to a cost random variable.
  //
  //  (7) To solve our optimization problem, namely:
  //
  //  Minimize  : Risk of time
  //  Subject to: Risk of violation < Cost
  //
  //  We could also try to solve the problem of:
  //  Minimize  : Risk of time + Risk of violation (?)
  //  We just have to find how many steps we need to "wait" before things go hairy.
  //
  //  We prune the plans with risk of violation < cost We measure the risk of
  //  time of each "plan" (around 65k in our case?) and the risk of violation
  //  of each plan and choose the best one and execute it.
  //
}

int main(int argc, char **argv) {
  TEST(jupiter_cluster);
  TEST(tm_read_load);
  TEST(tm_trace);
  TEST(ewma);
  TEST(group_state);
  TEST(dual_state);
  TEST(tri_state);

  //test_tri_state();
  return 0;
}
