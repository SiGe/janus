#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algo/maxmin.h"
#include "networks/jupiter.h"
#include "util/log.h"

#include "traffic.h"

#define RUN_COUNT 10

void traffic_matrix_random(
    struct traffic_matrix_t **ret, uint32_t num_tors, bw_t bw, float density) {

  uint32_t num_flows =  num_tors * num_tors * density;
  size_t size = sizeof(struct traffic_matrix_t) + \
               sizeof(struct pair_bw_t) * num_flows;
  struct traffic_matrix_t *tm = malloc(size);
  memset(tm, 0, size);

  (tm)->num_pairs = num_flows;
  //(*tm)->bws = (struct pair_bw_t *)(((char*) *tm) + sizeof(struct traffic_matrix_t));
  struct pair_bw_t *pair = (tm)->bws;

  for (uint32_t i = 0; i < num_flows; ++i) {
    pair->did = rand() % num_tors;
    pair->sid = rand() % num_tors;
    pair->bw = ((float)rand() / (float)RAND_MAX) * bw;
    pair++;
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
  uint32_t num_tors = 2; //48
  uint32_t num_cores = 4; //96

  num_pods = 32;
  num_aggs = 24;
  num_tors = 48;
  num_cores = 96;

  bw_t bw = 10;

  struct network_t *net = jupiter_network_create(
      num_cores, num_pods, num_aggs, num_tors, bw);

  struct traffic_matrix_t *tm = 0;

  struct dataplane_t dp = {0};
  traffic_matrix_random(&tm, num_tors * num_pods, bw, 0.2);
  jupiter_set_traffic(net, tm);

  for (uint32_t i = 0; i < num_aggs-5; ++i) {
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
    assert(p1->did == p2->did);
    assert(p1->sid == p2->sid);
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
    free(tm2);
  }
  fclose(f);
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

int main(int argc, char **argv) {
  info("%d", sizeof(struct pair_bw_t));
  //test_jupiter_cluster();
  // test_tm_read_load();
  //test_tm_trace();
  return 0;
}
