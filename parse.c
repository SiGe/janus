#include <ctype.h>
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

int read_uint32(char const *input, uint32_t *output) {
    uint32_t ret = 0;
    char const *begin = input;

    while (isdigit(*input)) {
        ret = ret * 10 + (*input - '0');
        input++;
    }
    *output = ret;
    return (input - begin);
}

int read_bw(char const *input, bw_t *output) {
    char const *begin = input;
    uint32_t whole = 0;
    int pos = read_uint32(input, &whole);
    input += pos;
    *output = (bw_t)(whole);
    if (*input != '.')
      return (input - begin);
    input++;
    bw_t power = 1;
    bw_t fraction = 0;
    while (isdigit(*input)) {
        fraction = fraction + (*input - '0')/power;
        power *= 10;
        input++;
    }
    *output += fraction;
    return (input - begin);
}

int read_link_id(char const *input, link_id_t *output) {
    char const *begin = input;
    link_id_t ret = 0;
    while (isdigit(*input)) {
        ret = ret * 10 + (*input - '0');
        input++;
    }
    *output = ret;
    return (input - begin);
}

char const *parse_routing_matrix(char const *input, struct network_t *network) {
  info("parsing routing.");
  uint32_t num_lines = 0;
  int pos = 0;

  /* read number of lines */
  pos = read_uint32(input, &num_lines);
  input += pos;
  input = strip(input);

  /* bypass newline */
  input++;

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
    link_id_t link;

    pos = read_link_id(input, &link);
    input += pos;
    input = strip(input);

    /* Save the link id */
    /* remember the max link id seen */
    if (pos > 0) {
      if (num_links < link) {
        num_links = link;
      }

      /* keep the first position for number of elements, hence the + 1 */
      *(out + index + 1) = link;
      index += 1;
    }

    /* Bypass white lines */
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

  bw_t bandwidth; int pos;
  for (pair_id_t i = 0; i < network->num_flows; i++) {
    pos = read_bw(input, &bandwidth);
    input += pos;
    input = strip(input);

    /* save the bandwidth and move the input pointer forward */
    flows->id = i; flows->demand = bandwidth; flows++;
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
  
  bw_t bandwidth; int pos;
  for (link_id_t i = 0; i < network->num_links; i++) {
    pos = read_bw(input, &bandwidth);
    input += pos;
    input = strip(input);

    /* save the bandwidth and move the input pointer forward */
    links->id = i; links->capacity = bandwidth; links->used = 0; links++;
  }

  if (*input != 0) input = strip(input)+1;
  return strip(input);
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

  return E_OK;
}
