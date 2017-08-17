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
  return network->smallest_flow;
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

static void linked_list_remove_flow(struct flow_t *flow) {
  if (flow->prev)
    flow->prev->next = flow->next;

  if (flow->next)
    flow->next->prev = flow->prev;
}

// fixes a flow by updating the links on its path
static int fix_flow(struct network_t *network, struct flow_t *flow) {
  for (int i = 0; i < flow->nlinks; i++) {
    struct link_t *link = flow->links[i];
    link->used += remaining_demand(flow);
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

  /* remove the flow from the chain of good flows */
  linked_list_remove_flow(flow);

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

    /* take it out of the loop */
    linked_list_remove_flow(flow);

    if (network->smallest_flow == flow) {
      network->smallest_flow = flow->next;
    }
  }

  return 1;
}

static int flow_cmp(void const *v1, void const *v2) {
  struct flow_t const *f1 = (struct flow_t const*)v1;
  struct flow_t const *f2 = (struct flow_t const*)v2;

  return (int)(f1->demand - f2->demand);
}

static __attribute__((unused)) int link_cmp(void const *v1, void const *v2) {
  struct link_t const *l1 = (struct link_t const*)v1;
  struct link_t const *l2 = (struct link_t const*)v2;

  return (int)(l1->capacity - l2->capacity);
}

static void populate_and_sort_flows(struct network_t *network) {
  /* populate the link and flow structures */
  link_id_t *ptr = network->routing;
  struct flow_t *flow = network->flows;
  for (int i = 0; i < network->num_flows; ++i, ++flow, ptr += (MAX_PATH_LENGTH + 1)) {
    /* if flow has less than EPS demand, just ignore it */
    if (flow->demand < EPS) {
      continue;
    }

    for (int j = 0; j < *ptr; ++j) {
      struct link_t *link = &network->links[*(ptr+j+1)];
      flow->links[flow->nlinks++] = link;
      link->nflows++;
    }
  }

  /* sort the flows by their demand */
  qsort(network->flows, network->num_flows, sizeof(struct flow_t), flow_cmp);

  flow = network->flows;
  for (int i = 0; i < network->num_flows; ++i, ++flow)  {
    if (flow->demand > EPS)
      break;
  }
  network->smallest_flow = flow;
}

static void populate_links(struct network_t *network) {
  /* populate the links with non-zero flows */
  struct flow_t *flow = network->flows;
  struct flow_t *prev = 0;
  for (int i = 0; i < network->num_flows; ++i, ++flow) {
    if (flow->demand < EPS) {
      continue;
    }

    for (int j = 0; j < flow->nlinks; ++j) {
      struct link_t *link = flow->links[j];
      /* create the flows data-structure if it doesn't exist */
      if (link->flows == 0) {
        link->flows = malloc(link->nflows * sizeof(struct flow_t *));
        link->nactive_flows = 0;
      }

      link->flows[link->nactive_flows++] = flow;
    }

    // set the proper id for this flow
    flow->id = i;

    // properly set the prev and next last flow id
    flow->prev = prev;

    if (prev)
      prev->next = flow;

    prev = flow;
  }
  flow->next = 0;
}

static void populate_flow_fixed_data_structure(struct network_t *network) {
  pair_id_t *flow_ids = malloc(sizeof(pair_id_t) * network->num_flows);
  for (int i = 0; i < network->num_flows; ++i) {
    flow_ids[i] = i;
  }
  network->flow_ids = flow_ids;
  network->fixed_flow_end = 0;
}

static void network_prepare(struct network_t *network) {
  populate_and_sort_flows(network);
  populate_links(network);
  populate_flow_fixed_data_structure(network);
}


/* calculate the max-min fairness of the network flows. This is a destructive
   operation---i.e., the network structure will change */
int maxmin(struct network_t *network) {
  network_prepare(network);

  while (1) {
    struct flow_t *flow = find_flow_with_smallest_remaining_demand(network);
    struct link_t *link = find_link_with_smallest_remaining_per_flow_bw(network);

    if (flow == 0 || link == 0)
      return 1;

    if (remaining_demand(flow) < per_flow_capacity(link)) {
      info("Fixing flow: %d", flow->id);
      fix_flow(network, flow);
      network->smallest_flow = flow->next;
    } else {
      info("Fixing link: %d (@%.2f)", link->id, per_flow_capacity(link));
      fix_link(network, link);
    }
  }
}
