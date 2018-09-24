#ifndef _PARSE_H_
#define _PARSE_H_

#include <stdint.h>

#include "types.h"

/* Max host count in the data-center */
#define MAX_HOST_COUNT 100

#define MAX_FILE_SIZE (1 << 30) * sizeof(char)

/* Markers in the input stream for parsing */
#define MARKER_ROUTING_MATRIX 'r'
#define MARKER_FLOW 'f'
#define MARKER_LINK 'l'

void read_file(char const *file, char **output);

/* Parse the input character stream and build three matrices for use in progressive_filling algorithm */
int parse_input(char const *input, struct network_t *network);

/* Parse the routing matrix */
char const *parse_routing_matrix(char const *input, struct network_t *network);
char const *parse_links(char const *input, struct network_t *network);
char const *parse_flows(char const *input, struct network_t *network);

/* ignore whitespaces and tabs */
char const *strip(char const *input);

int read_bw(char const *input, bw_t *value, int *pos);

int parse_error(char const *file_name, struct error_t *errors);
int parse_error_range(char const *folder_name, struct error_range_t *errors, int str, int end);

#endif /* _PARSE_H_ */
