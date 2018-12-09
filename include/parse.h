#ifndef _PARSE_H_
#define _PARSE_H_

#include <stdint.h>

#include "dataplane.h"

/* Max host count in the data-center */
#define MAX_HOST_COUNT 100


/* Markers in the input stream for parsing */
#define MARKER_ROUTING_MATRIX 'r'
#define MARKER_FLOW 'f'
#define MARKER_LINK 'l'

/* Parse the input character stream and build three matrices for use in progressive_filling algorithm */
int parse_input(char const *input, struct dataplane_t *);

/* Parse the routing matrix */
char const *parse_routing_matrix(char const *input, struct dataplane_t *);
char const *parse_links(char const *input, struct dataplane_t *);
char const *parse_flows(char const *input, struct dataplane_t *);

/* ignore whitespaces and tabs */
char const *strip(char const *input);

#endif /* _PARSE_H_ */
