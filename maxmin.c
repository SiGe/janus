#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <pthread.h>
#include <unistd.h>

#include "algorithm.h"
#include "error.h"
#include "log.h"
#include "parse.h"
#include "types.h"
#include "topo.h"
#include "traffic.h"
#include "thpool.h"

int network_slo_violation(struct network_t *network, double y) {
  struct flow_t *flow = 0;
  int vio_num = 0;
  for (int i = 0; i < network->num_flows; ++i) {
    flow = &network->flows[i];
    if ((flow->demand > flow->bw) && (flow->bw < y)) {
        vio_num += 1;
    }
    if ((flow->demand < flow->bw)) {
        panic("More bandwith than demand");
    }
  }
  //printf("%d ToR pairs violating %f bandwidth, %d ToR pairs permitted\n", vio_num, y, x);
  return vio_num;
  //printf("%d", vio_num);
}

void network_max_link_throughput(struct network_t *network) {
    struct link_t *link= 0;
    double max_link_throughput = 0.0;
    for(int i = 0; i < network->num_links; ++i) {
        link = &network->links[i];
        if (link->used / link->capacity > max_link_throughput)
            max_link_throughput = link->used / link->capacity;
    }
    printf("%f", max_link_throughput);
}

const char *usage_message = "" \
  "usage: %s <routing-file>\n" \
  "routing-file has the following format:\n\n" \
  "\tr\n"\
  "\t[num-flows]\n"\
  "\t[links on path of flow 0]\n"\
  "\t[links on path of flow 1]\n"\
  "\t[...]\n"\
  "\t[links on path of flow n]\n"\
  "\tl\n"\
  "\t[link capacities, space separated]\n"\
  "\tf\n"\
  "\t[flow demands, space separated]\n\n";

void usage(const char *fname) {
  printf(usage_message, fname);
  exit(EXIT_FAILURE);
}

struct parallel_param_t {
  int *symmetric_subplan;
  int groups_len;

  int *update_groups;

  struct traffic_t *traffic;

  int *data;
  int idx;
};

void parallel_calculate(void *vargp)
{
    struct parallel_param_t *param = vargp;
    struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);

    printf("%d\n", param->idx);

    int node_num = 0;
    for (int i = 0; i < param->groups_len; i++)
        node_num += param->symmetric_subplan[i];

    int *update_nodes = malloc(sizeof(int) * node_num);
    int update_nodes_pos = 0;
    for (int i = 0; i < param->groups_len; i++)
    {
        for (int j = 0; j < param->symmetric_subplan[i]; j++)
        {
            update_nodes[update_nodes_pos] = param->update_groups[i] + j;
            update_nodes_pos++;
        }
    }

    struct traffic_t *traffic = param->traffic;
    network_update(test_network, update_nodes, node_num);
    for (int i = 0; i < traffic->tm_num; i++)
    {
        network_reset(test_network);
        build_flow(test_network, traffic, i);
        maxmin(test_network);
        int ret = network_slo_violation(test_network, 6000000000);
        param->data[i] = ret;
    }
    network_free(test_network);
    free(vargp);
}

void all_subplan_seq_cost()
{
  int symmetric_groups[5] = {3,3,3,3,3};
  int update_groups[5] = {0, 6, 12, 42, 48};
  int tot_subplans = 0;
  int **symmetric_subplans = generate_subplan(symmetric_groups, 5, &tot_subplans);
  threadpool thpool = thpool_init(sysconf(_SC_NPROCESSORS_ONLN) - 1);
  struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);
  struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", test_network, 2500);
  int **data = malloc(sizeof(int *) * tot_subplans);

  for (int i = 0; i < tot_subplans; i++)
  {
      data[i] = malloc(sizeof(int) * traffic->tm_num);
      struct parallel_param_t *param = malloc(sizeof(struct parallel_param_t));
      param->symmetric_subplan = symmetric_subplans[i];
      param->groups_len = 5;
      param->update_groups = update_groups;
      param->traffic = traffic;
      param->data = data[i];
      param->idx = i;
      thpool_add_work(thpool, parallel_calculate, param);
  }
  thpool_wait(thpool);
  FILE *f = fopen("result.tsv", "w+");
  for (int i = 0; i < tot_subplans; i++)
  {
      for (int j = 0; j < traffic->tm_num; j++)
      {
          fprintf(f, "%d ", data[i][j]);
      }
      fprintf(f, "\n");
  }
  fclose(f);
}

void single_plan_seq_cost()
{
  int update_nodes[] = {0, 6, 7};
  struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);
  network_update(test_network, update_nodes, 10);
  struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", test_network, 2500);
  for (int i = 0; i < traffic->tm_num; i++)
  {
      network_reset(test_network);
      build_flow(test_network, traffic, i);
      maxmin(test_network);
      printf("%d ", network_slo_violation(test_network, 6000000000));
  }
  printf("\n");
  network_free(test_network);
} 

void plan_debug()
{
    int update_nodes[] = {0, 6, 7};
    struct network_t *test_network = network_watchtower_gen(8, 12, 6, 6);
    network_update(test_network, update_nodes, 3);
    struct traffic_t *traffic = traffic_load("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", test_network, 2500);
    int time = 944;
    network_reset(test_network);
    print_links(test_network);
    build_flow(test_network, traffic, time);
    maxmin(test_network);
    printf("%d\n", network_slo_violation(test_network, 6000000000));
    network_free(test_network);
}

int main(int argc, char **argv) {
  char *output = 0;
  int err = 0;
  int c = 0;
  int max_link_flag = 0;
  double y = -1;
  char *file_name = NULL;

  all_subplan_seq_cost();
  //single_plan_seq_cost();
  //plan_debug();
  exit(0);

  while ((c = getopt(argc, argv, "f:y:m")) != -1)
  {
    switch (c)
    {
        case 'f':
            file_name = optarg;
            break;
        case 'y':
            y = atof(optarg);
            break;
        case 'm':
            max_link_flag = 1;
            break;
        default:
            usage(argv[0]);
    }
  }

  info("reading data file.");
  read_file(file_name, &output);

  struct network_t network = {0};
  if ((err = parse_input(output, &network)) != E_OK) {
    error("failed to read the data file: %d.", err);
    return EXIT_FAILURE;
  };

  maxmin(&network);
  //network_print_flows(&network);
  //network_slo_violation(&network, atof(argv[2]));
  if (max_link_flag)
      network_max_link_throughput(&network);
  else if (y >= 0)
      printf("%d", network_slo_violation(&network, y));
  network_free(&network);

  return EXIT_SUCCESS;
}
