#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#include "algorithm.h"
#include "error.h"
#include "log.h"
#include "parse.h"
#include "types.h"
#include "topo.h"
#include "topo.h"
#include "traffic.h"

void network_slo_violation(struct network_t *network, double y) {
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
  printf("%d", vio_num);
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

int main(int argc, char **argv) {
  char *output = 0;
  int err = 0;
  int c = 0;
  int max_link_flag = 0;
  double y = -1;
  char *file_name = NULL;

  /* Tests written by Jiaqi */
  int update_nodes[] = {0, 1, 6, 7, 12, 13, 42, 43, 48, 49};
  //for (int i = 0; i < test_network->traffic->tm_num; i++)
  struct network_t *test_network = watchtower_gen(8, 12, 6, 6);
  update_network(test_network, update_nodes, 10);
  load_traffic("../traffic/webserver_traffic_30s_8p_12t_sorted.tsv", test_network, 2500);
  for (int i = 0; i < test_network->traffic->tm_num; i++)
  {
      reset_network(test_network);
      build_flow(test_network, i);
      //printf("C\n");
      maxmin(test_network);
      //printf("D\n");
      network_slo_violation(test_network, 6000000000);
      printf(" ");
  }
  printf("\n");
  network_free(test_network);
  exit(0);
  /* End of tests */

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
      network_slo_violation(&network, y);
  network_free(&network);

  return EXIT_SUCCESS;
}
