#ifndef _TOPO_H_
#define _TOPO_H_

#include "types.h"

/* Return a watchtower topology with routing without flows*/
struct network_t *network_watchtower_gen(int k, int t_per_p, int a_per_p, int c_num);

void network_update(struct network_t *network, int *node_ids, int node_nums);

void network_restore(struct network_t *network);

void network_reset(struct network_t *network);

void network_free(struct network_t *network);

void print_routing(struct network_t *network);

void print_links(struct network_t *network);

#endif
