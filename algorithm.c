#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "error.h"
#include "log.h"
#include "types.h"
#include "parse.h"

#include "algorithm.h"

#define EPS 1e-6

static inline bw_t max(bw_t a, bw_t b) {
  return  (a > b) ? a : b;
}

// returns the remaining_demand of a flow
static inline bw_t remaining_demand(struct flow_t *flow) {
  return flow->demand - flow->bw ;
}

// returns the capacity that a link can spare for each active flow (i.e., flows
// that are not bottlenecked in other parts of the network)
static inline bw_t per_flow_capacity(struct link_t *link) {
  return max((link->capacity - link->used) / link->nactive_flows, 0);
}

// finds the flow with the smallest remaining demand
static struct flow_t *find_flow_with_smallest_remaining_demand(struct network_t *network) {
  bw_t min_remaining_demand = INFINITY;
  struct flow_t *ret = 0;

  for (int i = network->fixed_flow_end; i < network->num_flows; ++i) {
    struct flow_t *f = &network->flows[network->flow_ids[i]];

    if (min_remaining_demand > f->demand - f->bw) {
      min_remaining_demand = f->demand - f->bw;
      ret = f;
    }
  }

  return ret;
}

// finds a link that gets saturated first
static struct link_t *find_link_with_smallest_remaining_per_flow_bw(struct network_t *network) {
  bw_t min_remaining_capacity = INFINITY;
  struct link_t *ret = 0;

  for (int i = network->fixed_link_end; i < network->num_links; ++i) {
    struct link_t *l = &network->links[i];
    if (l->nactive_flows == 0 || per_flow_capacity(l) < EPS)
      continue;

    bw_t metric = per_flow_capacity(l);

    if (min_remaining_capacity > metric) {
      min_remaining_capacity = metric;
      ret = l;
    }
  }

  return ret;
}

// marks a flow as fixed--i.e., his bw cannot be upgraded anymore
static void mark_flow_as_fixed(struct network_t *network, int flow_id) {
  pair_id_t *ptr = network->flow_ids + network->fixed_flow_end;
  for (int i = network->fixed_flow_end; i < network->num_flows; ++i) {
    if (*ptr == flow_id) {
      pair_id_t tmp = network->flow_ids[network->fixed_flow_end];
      info("swapping: [%d]==%d with [%d]==%d", network->fixed_flow_end, tmp, i, *ptr);
      network->flow_ids[network->fixed_flow_end] = *ptr;
      *ptr = tmp;
      network->fixed_flow_end++;
      return;
    }
    ptr++;
  }
  panic("flow was already fixed : %d, network->fixed_flow_end == %d", flow_id, network->fixed_flow_end);
}

// fixes a flow by updating the links on its path
static int fix_flow(struct network_t *network, struct flow_t *flow) {
  for (int i = 0; i < flow->nlinks; i++) {
    struct link_t *link = flow->links[i];
    link->used += remaining_demand(flow);
    if (link->id == 16) {
      info("fixing flow==%d on link->id == 16, active_flows=%d", flow->id, link->nactive_flows);
    }
    if (link->nactive_flows==0)
      panic("what the heck mate ...: %d, %d, %.2f, %.2f, %d, %d",
            link->id, flow->id,
            link->used, link->capacity,
            link->nflows, network->fixed_flow_end);

    link->nactive_flows -= 1;
  }

  flow->fixed = 1;
  mark_flow_as_fixed(network, flow->id);
  flow->bw = flow->demand;

  return 1;
}

// fixes a link by fixing the flows on itself and distributing the spare capacity that it has.
static int fix_link(struct network_t *network, struct link_t *link) {
  bw_t spare_capacity = per_flow_capacity(link);

  struct flow_t *flow = 0;
  pair_id_t nflows = link->nflows;
  for (int i = 0; i < nflows; ++i) {
    flow = link->flows[i];
    // TODO omida: maybe this shouldn't happen?
    if (flow->fixed) continue;

    struct link_t *l = 0;
    for (int j = 0; j < flow->nlinks; ++j) {
      l = flow->links[j];
      l->nactive_flows -= 1;
      l->used += spare_capacity;
    }

    flow->fixed = 1;
    mark_flow_as_fixed(network, flow->id);
    flow->bw += spare_capacity;

  }

  return 1;
}


/* calculate the max-min fairness of the network flows. This is a destructive
   operation---i.e., the network structure will change */
int maxmin(struct network_t *network) {
  while (1) {
    struct flow_t *flow = find_flow_with_smallest_remaining_demand(network);
    struct link_t *link = find_link_with_smallest_remaining_per_flow_bw(network);

    if (flow == 0 || link == 0)
      return 1;

    if (remaining_demand(flow) < per_flow_capacity(link)) {
      info("Fixing flow: %d", flow->id);
      fix_flow(network, flow);
    } else {
      info("Fixing link: %d (@%.2f)", link->id, per_flow_capacity(link));
      fix_link(network, link);
    }
  }
}
