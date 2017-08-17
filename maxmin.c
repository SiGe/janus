#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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


// TODO jiaqi: you can change the output format here. both network->flows and
// network->links are in the same order that you passed. To see what the
// structures have take a look at types.h
void network_print_flows(struct network_t *network) {
  printf("----------------------------------------\n");
  struct flow_t *flow = 0;
  for (int i = 0; i < network->num_flows; ++i) {
    flow = &network->flows[i];
    printf("flow %d: %.2f/%.2f (%.2f%%)\n", i, flow->bw, flow->demand, flow->bw/flow->demand * 100);
  }

  printf("----------------------------------------\n");
  for (int i = 0; i < network->num_links; ++i) {
    struct link_t *link = &network->links[i];
    printf("link %d: %.2f/%.2f (%.2f%%)\n", i, link->used, link->capacity, link->used/link->capacity * 100);
  }
  printf("----------------------------------------\n");
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

  if (argc < 2) {
    usage(argv[0]);
  }

  info("reading data file.");
  read_file(argv[1], &output);

  for (int i = 0; i < 100; ++i) {
      struct network_t network = {0};
      if ((err = parse_input(output, &network)) != E_OK) {
          error("failed to read the data file: %d.", err);
          return EXIT_FAILURE;
      };

      //network_print_flows(&network);
      maxmin(&network);
      //network_print_flows(&network);
      network_free(&network);
  }

  printf("successfully completed maxmin calculations.\n");

  return EXIT_SUCCESS;
}
