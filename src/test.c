#include <stdio.h>
#include <stdlib.h>

#include "networks/jupiter.h"
#include "algo/maxmin.h"

#define RUN_COUNT 10

void traffic_matrix_random(
    struct traffic_matrix_t **tm, uint32_t num_tors, bw_t bw, float density) {

  uint32_t num_flows =  num_tors * num_tors * density;
  *tm = malloc(sizeof(struct traffic_matrix_t) + \
               sizeof(struct pair_bw_t) * num_flows);

  (*tm)->num_pairs = num_flows;
  (*tm)->bws = (struct pair_bw_t *)(((char*) *tm) + sizeof(struct traffic_matrix_t));
  struct pair_bw_t *pair = (*tm)->bws;

  for (uint32_t i = 0; i < num_flows; ++i) {
    pair->did = rand() % num_tors;
    pair->sid = rand() % num_tors;
    pair->bw = ((float)rand() / (float)RAND_MAX) * bw;
    pair++;
  }
}

void network_stats(struct dataplane_t *network) {
  struct flow_t *flow = 0;
  for (int i = 0; i < network->num_flows; ++i) {
    flow = &network->flows[i];
    printf("Flow %d - bandwidth/demand: (%.2f/%.2f) = %.1f%%\n", 
        i, flow->bw, flow->demand, flow->bw/flow->demand * 100);
  }
}

int main(int argc, char **argv) {
  printf("Testing is initialized.");
  uint32_t num_pods = 12; //32
  uint32_t num_aggs = 6;  //24
  uint32_t num_tors = 12; //48
  uint32_t num_cores = 4; //96

  num_pods = 32;
  num_aggs = 24;
  num_tors = 48;
  num_cores = 96;

  bw_t bw = 10;

  struct network_t *net = jupiter_network_create(
      num_cores, num_pods, num_aggs, num_tors, bw);

  struct dataplane_t dp = {0};
  traffic_matrix_random(&net->tm, num_tors * num_pods, bw, 0.2);

  for (uint32_t i = 0; i < num_aggs-5; ++i) {
    jupiter_drain_switch(net, 
        jupiter_get_agg(net, 0, i));
  }

  for (uint32_t i = 0; i < RUN_COUNT; ++i) {
    jupiter_get_dataplane(net, (&dp));
    maxmin(&dp);
  }
  // network_stats(&dp);
  dataplane_init(&dp);
  free(net->tm);
  jupiter_network_free((struct jupiter_network_t*)net);
  return 0;
}
