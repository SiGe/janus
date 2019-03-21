#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dataplane.h"
#include "util/error.h"
#include "util/log.h"

#include "algo/maxmin.h"

#define EPS 1e-2

static inline bw_t max(bw_t a, bw_t b) {
  return  (a > b) ? a : b;
}

// returns the remaining_demand of a flow
static inline bw_t remaining_demand(struct flow_t const *flow) {
  return flow->demand - flow->bw ;
}

// returns the capacity that a link can spare for each active flow (i.e., flows
// that are not bottlenecked in other parts of the dataplane)
static inline bw_t per_flow_capacity(struct link_t const *link) {
  if (link->nactive_flows == 0)
    return 0;
  if (link->capacity == 0)
    return 0;
  return max((link->capacity - link->used) / link->nactive_flows, 0);
}

static __attribute__((unused)) void ensure_consistency_of_links(struct dataplane_t *dataplane) {
  bw_t last = 0;
  int loop = 0;
  struct link_t *link = dataplane->smallest_link;
  while (link) {
    loop++;
    if (last > per_flow_capacity(link))
      panic("this should not happen: %d, %.2f, %.2f", loop, last, per_flow_capacity(link));
    last = per_flow_capacity(link);
    link = link->next;
  }
}

static __attribute__((unused)) void dataplane_smallest(struct dataplane_t *dataplane) {
  printf("smallest link chain: \n");
  struct link_t *link = dataplane->smallest_link;
  while (link) {
    printf("(%d: "BWF") ~> \n", link->id, per_flow_capacity(link));
    if (link->next)
      if (link->next->prev != link) {
        assert(link->next->prev == link);
      }
    link = link->next;
  }
  printf("\n");
}

static __attribute__((unused)) void dataplane_consistent(struct dataplane_t *dataplane) {
  struct link_t *link = dataplane->smallest_link;
  while (link) {
    if (link->next)
      if (link->next->prev != link) {
        printf("inconsistency: %d <-> %d\n", link->id, link->next->id);
        assert(link->next->prev == link);
      }
    link = link->next;
  }
  info_txt("dataplane-consistent.");
}

static __attribute__((unused)) int is_28_there(struct dataplane_t *dataplane) {
  struct link_t *link = dataplane->smallest_link;
  while (link) {
    if (link->id == 28)
      return 1;
    link = link->next;
  }
  return 0;
}


// finds the flow with the smallest remaining demand
static struct flow_t *find_flow_with_smallest_remaining_demand(struct dataplane_t *dataplane) {
  return dataplane->smallest_flow;
}

// finds a link that gets saturated first
static struct link_t *find_link_with_smallest_remaining_per_flow_bw(struct dataplane_t *dataplane) {
  return dataplane->smallest_link;
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

static void cut_link(struct link_t *l) {
  linked_list_remove_link(l);
}

static void stitch_link_after(struct link_t *l, struct link_t *after) {
  //info("AFTER %4d (%15.2f) after %4d (%15.2f)", l->id, per_flow_capacity(l), after->id, per_flow_capacity(after));
  l->next = after->next;
  l->prev = after;

  if (after->next)
    after->next->prev = l;
  after->next = l;
  //info("after->next (%d) = l (%d), l->prev (%d) = after->id (%d)", after->next->id, l->id, l->prev->id, after->id);
}

static void stitch_link_before(struct link_t *l, struct link_t *before) {
  //info("BEFORE %4d (%15.2f) before %4d (%15.2f)", l->id, per_flow_capacity(l), before->id, per_flow_capacity(before));
  l->next = before;
  l->prev = before->prev;

  if (before->prev)
    before->prev->next = l;
  before->prev = l;
}

static void recycle_link_if_fixed(struct dataplane_t *dataplane, struct link_t *l) {
  if (l->nactive_flows == 0) {
    if (dataplane->smallest_link == l) {
      dataplane->smallest_link = l->next;
    }

    linked_list_remove_link(l);
    return;
  }

  // If l->prev
  if (l->prev && (per_flow_capacity(l) < per_flow_capacity(l->prev))) {
    // Do surgery to find the correct position of the link
    struct link_t *prv = l->prev;
    cut_link(l);
    while (prv->prev && per_flow_capacity(l) < per_flow_capacity(prv->prev)) {
      prv = prv->prev;
    }
    stitch_link_before(l, prv);

    if (l->prev == 0 && l->nactive_flows != 0) {
      dataplane->smallest_link = l;
    }

    return;
  }

  if (l->next && (per_flow_capacity(l) > per_flow_capacity(l->next))) {
    if (dataplane->smallest_link == l) {
      dataplane->smallest_link = l->next;
    }

    // Do surgery to find the correct position of the link
    struct link_t *nxt = l->next;
    cut_link(l);
    while (nxt->next && per_flow_capacity(l) > per_flow_capacity(nxt->next)) {
      nxt = nxt->next;
    }
    stitch_link_after(l, nxt);

    return;
  }
}

// fixes a flow by updating the links on its path
static int fix_flow(struct dataplane_t *dataplane, struct flow_t *flow) {
  for (int i = 0; i < flow->nlinks; i++) {
    struct link_t *link = flow->links[i];
    link->used += remaining_demand(flow);
    if (link->used > link->capacity) {
      dataplane_smallest(dataplane);
      panic("Trying to route on a link that has no space left: (Link) %d,\
          (Flow) %d, (Used cap) %.2f, (Total cap) %.2f, (Num flows on link) %d,\
          (Routed flows?) %d, (Remaining bandwidth on the flow) %.2f",
            link->id, flow->id, link->used, link->capacity, link->nflows,
            link->nactive_flows, remaining_demand(flow));
    }

    if (link->nactive_flows==0)
      panic("No flow left to route for link: (Link) %d, (Flow) %d, (Link used?)\
          %.2f, (Link cap) %.2f, (Num flows on link) %d, (Useless) %d",
            link->id, flow->id, link->used, link->capacity, link->nflows,
            dataplane->fixed_flow_end);

    link->nactive_flows -= 1;
    recycle_link_if_fixed(dataplane, link);
  }

  flow->fixed = 1;
  flow->bw = flow->demand;

  /* remove the flow from the chain of good flows */
  linked_list_remove_flow(flow);
  return 1;
}

// fixes a link by fixing the flows on itself and distributing the spare capacity that it has.
static int fix_link(struct dataplane_t *dataplane, struct link_t *link) {
  bw_t spare_capacity = per_flow_capacity(link);

  struct flow_t *flow = 0;
  pair_id_t nflows = link->nflows;

  // fix all the flows on the link
  for (int i = 0; i < nflows; ++i) {
    flow = link->flows[i];
    // TODO omida: maybe this shouldn't happen?
    if (flow->fixed) continue;

    struct link_t *l = 0;
    for (int j = 0; j < flow->nlinks; ++j) {
      l = flow->links[j];
      l->nactive_flows -= 1;
      l->used += spare_capacity;

      if ((l->used - l->capacity) > EPS) {
        // we can honestly set the l used to be equal to l capacity ...
        l->used = l->capacity;
        // dataplane_smallest(dataplane);
        // panic("Routing on a link that has no space left: (Link) %d, (Flow) %d,\
        //     (Used cap) %.2f, (Total cap) %.2f, (Num flows on link) %d, (Num\
        //       flows left) %d, (Remaining demand on flow) %.2f",
        //     l->id, flow->id, l->used, l->capacity, l->nflows, l->nactive_flows,
        //     remaining_demand(flow));
      }

      recycle_link_if_fixed(dataplane, l);
    }

    flow->fixed = 1;
    flow->bw += spare_capacity;

    /* take it out of the loop */
    linked_list_remove_flow(flow);

    if (dataplane->smallest_flow == flow) {
      dataplane->smallest_flow = flow->next;
    }
  }

  return 1;
}

static int flow_cmp(void const *v1, void const *v2) {
  struct flow_t const *f1 = (struct flow_t const*)v1;
  struct flow_t const *f2 = (struct flow_t const*)v2;

  bw_t val = (f1->demand - f2->demand);

  if (val < 0) {
    return -1;
  } else if (val > 0){
    return 1;
  }
  return 0;
}

static int link_cmp_ptr(void const *v1, void const *v2) {
  struct link_t const *l1 = *(struct link_t const**)v1;
  struct link_t const *l2 = *(struct link_t const**)v2;

  bw_t val = per_flow_capacity(l1) - per_flow_capacity(l2);

  if (val < 0) {
    return -1;
  } else if (val > 0){
    return 1;
  }
  return 0;
}

static void populate_and_sort_flows(struct dataplane_t *dataplane) {
  /* populate the link and flow structures */
  link_id_t *ptr = dataplane->routing;
  struct flow_t *flow = dataplane->flows;
  for (int i = 0; i < dataplane->num_flows; ++i, ++flow, ptr += (MAX_PATH_LENGTH + 1)) {
    /* if flow has less than EPS demand, just ignore it */
    if (flow->demand < EPS) {
      continue;
    }

    for (int j = 0; j < *ptr; ++j) {
      struct link_t *link = &dataplane->links[*(ptr+j+1)];
      flow->links[flow->nlinks++] = link;
      link->nflows++;
    }
  }

  /* sort the flows by their demand */
  qsort(dataplane->flows, dataplane->num_flows, sizeof(struct flow_t), flow_cmp);

  flow = dataplane->flows;
  for (int i = 0; i < dataplane->num_flows; ++i, ++flow)  {
    if (flow->demand > EPS)
      break;
  }
  dataplane->smallest_flow = flow;
}

static void populate_and_sort_links(struct dataplane_t *dataplane) {
  /* populate the links with non-zero flows */
  {
    struct flow_t *flow = dataplane->flows;
    struct flow_t *prev = 0;
    for (unsigned i = 0; i < dataplane->num_flows; ++i, ++flow) {
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
    prev->next = 0;
  }


  {
    /* create sortable links */
    struct link_t **links = malloc(sizeof(struct link_t *) * dataplane->num_links);
    struct link_t *link = dataplane->links;
    for (int i = 0; i < dataplane->num_links; ++i, ++link) {
      links[i] = link;
    }
    qsort(links, dataplane->num_links, sizeof(struct link_t *), link_cmp_ptr);
    struct link_t *prev = 0;
    dataplane->smallest_link = 0;

    for (int i = 0; i < dataplane->num_links; ++i) {
      link = links[i];
      link->next = link->prev = 0;
      if (link->nactive_flows == 0) {
        continue;
      }

      if (prev)
        prev->next = link;
      link->prev = prev;
      prev = link;

      if (!dataplane->smallest_link) {
        dataplane->smallest_link = link;
      }
    }
    prev->next = 0;
    free(links);
  }
}

static void dataplane_prepare(struct dataplane_t *dataplane) {
  populate_and_sort_flows(dataplane);
  populate_and_sort_links(dataplane);
}


/* calculate the max-min fairness of the dataplane flows. This is a destructive
   operation---i.e., the dataplane structure will change */
int maxmin(struct dataplane_t *dataplane) {
  dataplane_prepare(dataplane);

  while (1) {
    struct flow_t *flow = find_flow_with_smallest_remaining_demand(dataplane);
    struct link_t *link = find_link_with_smallest_remaining_per_flow_bw(dataplane);

    if (flow == 0 || link == 0)
      return 1;

    if (remaining_demand(flow) < per_flow_capacity(link)) {
      // info("fixing flow: %d", flow->id);
      fix_flow(dataplane, flow);
      dataplane->smallest_flow = flow->next;
    } else {
      // info("> fixing link: %d", link->id);
      fix_link(dataplane, link);
      // info("> done fixing link: %d", link->id);
    }
  }
}
