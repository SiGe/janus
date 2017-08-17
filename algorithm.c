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
static inline bw_t remaining_demand(struct flow_t const *flow) {
  return flow->demand - flow->bw ;
}

// returns the capacity that a link can spare for each active flow (i.e., flows
// that are not bottlenecked in other parts of the network)
static inline bw_t per_flow_capacity(struct link_t const *link) {
  if (link->nactive_flows == 0)
    return 0;
  return max((link->capacity - link->used) / link->nactive_flows, 0);
}

static __attribute__((unused)) void ensure_consistency_of_links(struct network_t *network) {
  bw_t last = 0;
  int loop = 0;
  struct link_t *link = network->smallest_link;
  while (link) {
    loop++;
    if (last > per_flow_capacity(link))
      panic("this should not happen: %d, %.2f, %.2f", loop, last, per_flow_capacity(link));
    last = per_flow_capacity(link);
    link = link->next;
  }
}


// finds the flow with the smallest remaining demand
static struct flow_t *find_flow_with_smallest_remaining_demand(struct network_t *network) {
  return network->smallest_flow;
}

// finds a link that gets saturated first
static struct link_t *find_link_with_smallest_remaining_per_flow_bw(struct network_t *network) {
  return network->smallest_link;
}

static void linked_list_remove_flow(struct flow_t *flow) {
  if (flow->prev)
    flow->prev->next = flow->next;

  if (flow->next)
    flow->next->prev = flow->prev;
}

static void linked_list_remove_link(struct link_t *link) {
  if (link->prev)
    link->prev->next = link->next;

  if (link->next)
    link->next->prev = link->prev;
}

static void recycle_link_if_fixed(struct network_t *network, struct link_t *l) {
  if (l->nactive_flows == 0) {
    if (network->smallest_link == l) {
      if (l->prev)
        network->smallest_link = l->prev;
      else
        network->smallest_link = l->next;
    }

    linked_list_remove_link(l);
    return;
  }

  // If l->prev
  if (l->prev && (per_flow_capacity(l) < per_flow_capacity(l->prev))) {
    // move l backwards
    while (l->prev && per_flow_capacity(l) < per_flow_capacity(l->prev)) {
      struct link_t *nxt = l->next;
      struct link_t *prv = l->prev;

      l->next = prv;
      l->prev = prv->prev;
      if (l->prev)
        l->prev->next = l;

      prv->next = nxt;
      prv->prev = l;

      if (nxt) {
        nxt->prev = prv;
      }
    }

    if (l->prev == 0 && l->nactive_flows != 0) {
      network->smallest_link = l;
    }

    return;
  }

  if (l->next && (per_flow_capacity(l) > per_flow_capacity(l->next))) {
    // move l forwards
    while (l->next && per_flow_capacity(l) > per_flow_capacity(l->next)) {
      struct link_t *nxt = l->next;
      struct link_t *prv = l->prev;

      l->next = nxt->next;
      if (nxt->next)
        nxt->next->prev = l;

      l->prev = nxt;
      nxt->prev = prv;
      nxt->next = l;

      if (prv) {
        prv->next = nxt;
      }
    }
    return;
  }
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
    recycle_link_if_fixed(network, link);
  }

  flow->fixed = 1;
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

      recycle_link_if_fixed(network, l);
    }

    flow->fixed = 1;
    flow->bw += spare_capacity;

    /* take it out of the loop */
    linked_list_remove_flow(flow);

    if (network->smallest_flow == flow) {
      network->smallest_flow = flow->next;
    }
  }

  recycle_link_if_fixed(network, link);
  return 1;
}

static int flow_cmp(void const *v1, void const *v2) {
  struct flow_t const *f1 = (struct flow_t const*)v1;
  struct flow_t const *f2 = (struct flow_t const*)v2;

  return (int)(f1->demand - f2->demand);
}

static int link_cmp(void const *v1, void const *v2) {
  struct link_t const *l1 = *(struct link_t const**)v1;
  struct link_t const *l2 = *(struct link_t const**)v2;

  return (int)((per_flow_capacity(l1)) - (per_flow_capacity(l2)));
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
    info("flow %d [link %d]", i, *ptr);

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

static void populate_and_sort_links(struct network_t *network) {
  /* populate the links with non-zero flows */
  {
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

  {
    /* create sortable links */
    struct link_t **links = malloc(sizeof(struct link_t *) * network->num_links);
    struct link_t *link = network->links;
    for (int i = 0; i < network->num_links; ++i, ++link) {
      links[i] = link;
    }

    qsort(links, network->num_links, sizeof(struct link_t *), link_cmp);
    struct link_t *prev = 0;
    network->smallest_link = 0;

    for (int i = 0; i < network->num_links; ++i) {
      link = links[i];
      link->next = link->prev = 0;
      if (link->nactive_flows == 0) {
        continue;
      }

      if (prev)
        prev->next = link;
      link->prev = prev;
      prev = link;

      if (!network->smallest_link) {
        network->smallest_link = link;
      }
    }
    link->next = 0;
    free(links);
  }
}

static void network_prepare(struct network_t *network) {
  populate_and_sort_flows(network);
  populate_and_sort_links(network);
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
      fix_flow(network, flow);
      network->smallest_flow = flow->next;
    } else {
      fix_link(network, link);
    }
  }
}
