#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "log.h"
#include "parse.h"
#include "types.h"


char const *strip(char const* input) {
  for (;*input == '\t' || *input == ' '; input++){}
  return input;
}

char const *parse_routing_matrix(char const *input, struct network_t *network) {
  info("parsing routing.");
  uint32_t num_lines = 0;
  int result = 0, pos = 0;

  /* read number of lines */
  result = sscanf(input, "%d%n", &num_lines, &pos);
  if (result == 0) {
    return input;
  }

  /* move the pointer forward */
  input += pos;

  /* allocate space for saving the matrix */
  link_id_t *out = malloc(sizeof(link_id_t) * (MAX_PATH_LENGTH+1) * num_lines);
  network->routing = out;

  int index = 0;
  int lines_read = 0;
  link_id_t num_links = 0;

  /* parse the input */
  while (1) {
    input = strip(input);

    /* read a link id */
    link_id_t link; result = sscanf(input, "%hd%n", &link, &pos);
    if (result <= 0) {
      network->num_links = 0;
      network->num_flows = 0;

      // Free the space and return 0;
      free(network->routing);
      network->routing = 0;
      return input;
    }

    input += pos;
    input = strip(input);


    /* Save the link id */
    if (result >= 1) {
      /* remember the max link id seen */
      if (num_links < link) {
        num_links = link;
      }

      /* keep the first position for number of elements, hence the + 1 */
      *(out + index + 1) = link;

      /* move index forward */
      index += 1;
    }

    /* By pass white lines */
    while (*input == '\n' || *input == 0) {
      /* save the number of links read */
      *out = index;

      /* and move forward */
      out += (MAX_PATH_LENGTH + 1);

      /* bypass the new line */
      input = strip(input);
      input += 1;

      /* set the index back to 0 */
      index = 0;

      /* if we have read all the lines, return */
      lines_read += 1;

      if (lines_read >= num_lines) {
        network->num_flows = lines_read;
        network->num_links = num_links + 1 /* offset the zero based indexing */;
        return input;
      }
    }
  }

  /* Should never reach here */
  network->routing = 0;
  return 0;
}

char const *parse_flows(char const *input, struct network_t *network) {
  info("parsing flows.");
  if (network->num_flows <= 0) {
    return input;
  }

  /* create space for flows */
  struct flow_t *flows = malloc(sizeof(struct flow_t) * network->num_flows);
  memset(flows, 0, sizeof(struct flow_t) * network->num_flows);
  network->flows = flows;
  int parsed_flows = 0;

  bw_t bandwidth; int pos, result;
  for (pair_id_t i = 0; i < network->num_flows; i++) {
    result = sscanf(input, "%lf%n", &bandwidth, &pos);
    if (result <= 0) {
      error("expected %d flows, got %d flows (input: %s).", network->num_flows, parsed_flows, input);
      free(network->flows);
      network->flows = 0;
      return input;
    }

    /* save the bandwidth and move the input pointer forward */
    flows->id = i; flows->demand = bandwidth; flows++;
    input += pos;
    parsed_flows++;
  }

  if (*input != 0) input = strip(input)+1;
  return strip(input);
}

char const *parse_links(char const *input, struct network_t *network) {
  info("parsing links.");
  if (network->num_links <= 0) {
    return input;
  }

  /* create space for links */
  struct link_t *links = malloc(sizeof(struct link_t) * network->num_links);
  memset(links, 0, sizeof(struct link_t) * network->num_links);
  network->links = links;

  bw_t bandwidth; int pos, result;
  for (link_id_t i = 0; i < network->num_links; i++) {
    result = sscanf(input, "%lf%n", &bandwidth, &pos);
    if (result <= 0) {
      error("error reading input: %s", input);
      free(network->links);
      network->links = 0;
      return input;
    }

    /* save the bandwidth and move the input pointer forward */
    links->id = i; links->capacity = bandwidth; links->used = 0; links++;
    input += pos;
  }

  if (*input != 0) input = strip(input)+1;
  return strip(input);
}

void _build_network(struct network_t *network) {
  link_id_t *ptr = network->routing;
  struct flow_t *flow = network->flows;
  for (int i = 0; i < network->num_flows; ++i) {
    for (int j = 0; j < *ptr; ++j) {
      struct link_t *link = &network->links[*(ptr+j+1)];
      flow->links[flow->nlinks++] = link;
      link->nflows++;
    }

    ptr += (MAX_PATH_LENGTH + 1);
    flow += 1;
  }

  flow = network->flows;
  for (int i = 0; i < network->num_flows; ++i) {
    for (int j = 0; j < flow->nlinks; ++j) {
      struct link_t *link = flow->links[j];
      if (link->flows == 0) {
        link->flows = malloc(link->nflows * sizeof(struct flow_t *));
        link->nactive_flows = 0;
      }

      link->flows[link->nactive_flows++] = flow;
    }
    flow += 1;
  }

  pair_id_t *flow_ids = malloc(sizeof(pair_id_t) * network->num_flows);
  for (int i = 0; i < network->num_flows; ++i) {
    flow_ids[i] = i;
  }
  network->flow_ids = flow_ids;
  network->fixed_flow_end = 0;

}

int parse_input(char const *input, struct network_t *network) {
  char const *ptr = input;

  /* Initialize everything to nil */
  network->routing = 0;
  network->links = 0;
  network->flows = 0;

  /* Parse routing/flow/or link values */
  while (*ptr != 0) {
    ptr = strip(ptr);
    char cmd = *ptr;
    // ignore the cmd character and the \n 
    ptr += 2;

    switch (cmd) {
    case MARKER_ROUTING_MATRIX:
      ptr = parse_routing_matrix(ptr, network);
      if (network->routing == 0) {
        return E_PARSE_ROUTING;
      }
      break;
    case MARKER_FLOW:
      ptr = parse_flows(ptr, network);
      if (network->flows == 0) {
        return E_PARSE_FLOW;
      }
      break;
    case MARKER_LINK:
      ptr = parse_links(ptr, network);
      if (network->links == 0) {
        return E_PARSE_LINK;
      }
      break;
    default:
      error("unexpected input: %d", cmd);
      return E_PARSE_UNEXPECTED;
    }
  }

  _build_network(network);
  return E_OK;
}
