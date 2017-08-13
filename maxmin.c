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


int main(int argc, char **argv) {
  char *output = 0;
  int err = 0;

  info("reading data file.");
  read_file("data/test_input_02.dat", &output);
  struct network_t network = {0};
  if ((err = parse_input(output, &network)) != E_OK) {
    error("failed to read the data file: %d.", err);
    return -1;
  };

  bw_t *flows = 0;
  maxmin(&network);
  //network_print_flows(&network);
  network_free(&network);
  return 0;
}
