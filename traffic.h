#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include "types.h"

struct traffic_t *traffic_load(char const *tracefile, struct network_t *network, int coeff);

void build_flow(struct network_t *network, struct traffic_t *traffic, int time);

void update_tm(struct network_t *network, struct tm_t *tm);

void print_flows(struct network_t *network);

void write_flows(struct network_t *network);

void build_flow_error(struct network_t *network, struct tm_t *tm, struct error_t *error, int error_seq, int tm_seq);

struct tm_t **build_flow_ewma(struct traffic_t *traffic, int time, int sd_pair_num);

#endif
