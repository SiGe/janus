#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include "types.h"

struct traffic_t *traffic_load(char const *tracefile, struct network_t *network, int coeff);

void build_flow(struct network_t *network, struct traffic_t *traffic, int time);

void print_flows(struct network_t *network);

#endif
