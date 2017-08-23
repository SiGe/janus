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

#define MAX_FILE_SIZE (1 << 20) * sizeof(char)
void read_file(char const *file, char **output) {
  FILE *f = fopen(file, "r+");
  if (!f) {
    return;
  }

  *output = malloc(MAX_FILE_SIZE);
  memset(*output, 0, MAX_FILE_SIZE);
  int nread = fread(*output, 1, MAX_FILE_SIZE, f);

  if (nread >= MAX_FILE_SIZE) {
    panic("file size too large (> %d bytes).", MAX_FILE_SIZE);
  }
  fclose(f);
}

void network_free(struct network_t *network) {
  if (network->routing) {
    free(network->routing);
    network->routing = 0;
  }

  if (network->links) {
    for (int i = 0; i < network->num_links; ++i) {
      free(network->links[i].flows);
    }
    free(network->links);
    network->links = 0;
  }

  if (network->flows) {
    free(network->flows);
    network->flows = 0;
  }
}

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
