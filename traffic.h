#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include "types.h"

void load_traffic(char const *tracefile, struct network_t *network, int coeff);

void build_flow(struct network_t *network, int time);

void print_flows(struct network_t *network);

#endif
