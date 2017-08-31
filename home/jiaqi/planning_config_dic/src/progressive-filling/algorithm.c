               :#include <assert.h>
               :#include <math.h>
               :#include <stdio.h>
               :#include <stdlib.h>
               :#include <string.h>
               :
               :#include "error.h"
               :#include "log.h"
               :#include "types.h"
               :#include "parse.h"
               :
               :#include "algorithm.h"
               :
               :#define EPS 1e-3
               :
               :static inline bw_t max(bw_t a, bw_t b) {
374553  0.2479 :  return  (a > b) ? a : b;
               :}
               :
               :// returns the remaining_demand of a flow
               :static inline bw_t remaining_demand(struct flow_t const *flow) {
  2620  0.0017 :  return flow->demand - flow->bw ;
               :}
               :
               :// returns the capacity that a link can spare for each active flow (i.e., flows
               :// that are not bottlenecked in other parts of the network)
               :static inline bw_t per_flow_capacity(struct link_t const *link) {
145505  0.0963 :  if (link->nactive_flows == 0)
               :    return 0;
263366  0.1743 :  return max((link->capacity - link->used) / link->nactive_flows, 0);
               :}
               :
               :static __attribute__((unused)) void ensure_consistency_of_links(struct network_t *network) {
               :  bw_t last = 0;
               :  int loop = 0;
               :  struct link_t *link = network->smallest_link;
               :  while (link) {
               :    loop++;
               :    if (last > per_flow_capacity(link))
               :      panic("this should not happen: %d, %.2f, %.2f", loop, last, per_flow_capacity(link));
               :    last = per_flow_capacity(link);
               :    link = link->next;
               :  }
               :}
               :
               :static __attribute__((unused)) void network_smallest(struct network_t *network) {
               :  printf("smallest link chain: \n");
               :  struct link_t *link = network->smallest_link;
               :  while (link) {
               :    printf("(%d: %.2f) ~> \n", link->id, per_flow_capacity(link));
               :    if (link->next)
               :      if (link->next->prev != link) {
               :        assert(link->next->prev == link);
               :      }
               :    link = link->next;
               :  }
               :  printf("\n");
               :}
               :
               :static __attribute__((unused)) void network_consistent(struct network_t *network) {
               :  struct link_t *link = network->smallest_link;
               :  while (link) {
               :    if (link->next)
               :      if (link->next->prev != link) {
               :        printf("inconsistency: %d <-> %d\n", link->id, link->next->id);
               :        assert(link->next->prev == link);
               :      }
               :    link = link->next;
               :  }
               :  info("network-consistent.");
               :}
               :
               :static __attribute__((unused)) int is_28_there(struct network_t *network) {
               :  struct link_t *link = network->smallest_link;
               :  while (link) {
               :    if (link->id == 28)
               :      return 1;
               :    link = link->next;
               :  }
               :  return 0;
               :}
               :
               :
               :// finds the flow with the smallest remaining demand
               :static struct flow_t *find_flow_with_smallest_remaining_demand(struct network_t *network) {
               :  return network->smallest_flow;
               :}
               :
               :// finds a link that gets saturated first
               :static struct link_t *find_link_with_smallest_remaining_per_flow_bw(struct network_t *network) {
               :  return network->smallest_link;
               :}
               :
               :static void linked_list_remove_flow(struct flow_t *flow) {
   270 1.8e-04 :  if (flow->prev)
               :    flow->prev->next = flow->next;
               :
  3899  0.0026 :  if (flow->next)
    60 4.0e-05 :    flow->next->prev = flow->prev;
               :}
               :
               :static void linked_list_remove_link(struct link_t *link) {
 10062  0.0067 :  if (link->prev)
   698 4.6e-04 :    link->prev->next = link->next;
               :
   243 1.6e-04 :  if (link->next)
 18916  0.0125 :    link->next->prev = link->prev;
               :}
               :
               :static void cut_link(struct link_t *l) {
               :  linked_list_remove_link(l);
               :}
               :
               :static void stitch_link_after(struct link_t *l, struct link_t *after) {
  8655  0.0057 :  info("AFTER %4d (%15.2f) after %4d (%15.2f)", l->id, per_flow_capacity(l), after->id, per_flow_capacity(after));
  3754  0.0025 :  l->next = after->next;
   213 1.4e-04 :  l->prev = after;
               :
  7752  0.0051 :  if (after->next)
   110 7.3e-05 :    after->next->prev = l;
  4567  0.0030 :  after->next = l;
 15962  0.0106 :  info("after->next (%d) = l (%d), l->prev (%d) = after->id (%d)", after->next->id, l->id, l->prev->id, after->id);
               :}
               :
               :static void stitch_link_before(struct link_t *l, struct link_t *before) {
               :  info("BEFORE %4d (%15.2f) before %4d (%15.2f)", l->id, per_flow_capacity(l), before->id, per_flow_capacity(before));
               :  l->next = before;
     2 1.3e-06 :  l->prev = before->prev;
               :
     1 6.6e-07 :  if (before->prev)
               :    before->prev->next = l;
               :  before->prev = l;
               :}
               :
 39798  0.0263 :static void recycle_link_if_fixed(struct network_t *network, struct link_t *l) { /* recycle_link_if_fixed.isra.5 total: 1144227  0.7573 */
 14479  0.0096 :  if (l->nactive_flows == 0) {
    84 5.6e-05 :    if (network->smallest_link == l) {
     5 3.3e-06 :      network->smallest_link = l->next;
               :    }
               :
               :    linked_list_remove_link(l);
               :    return;
               :  }
               :
               :  // If l->prev
 13786  0.0091 :  if (l->prev && (per_flow_capacity(l) < per_flow_capacity(l->prev))) {
               :    // Do surgery to find the correct position of the link
               :    struct link_t *prv = l->prev;
               :    cut_link(l);
    91 6.0e-05 :    while (prv->prev && per_flow_capacity(l) < per_flow_capacity(prv->prev)) {
               :      prv = prv->prev;
               :    }
               :    stitch_link_before(l, prv);
               :
               :    if (l->prev == 0 && l->nactive_flows != 0) {
               :      network->smallest_link = l;
               :    }
               :
               :    return;
               :  }
               :
 27991  0.0185 :  if (l->next && (per_flow_capacity(l) > per_flow_capacity(l->next))) {
   521 3.4e-04 :    if (network->smallest_link == l) {
    36 2.4e-05 :      network->smallest_link = l->next;
               :    }
               :
               :    // Do surgery to find the correct position of the link
               :    struct link_t *nxt = l->next;
               :    cut_link(l);
196964  0.1304 :    while (nxt->next && per_flow_capacity(l) > per_flow_capacity(nxt->next)) {
               :      nxt = nxt->next;
               :    }
               :    stitch_link_after(l, nxt);
               :
               :    //printf("(3) [%d], [m%d] %15.2f vs. smallest, %15.2f\n", is_28_there(network), l->id, per_flow_capacity(l), per_flow_capacity(network->smallest_link));
               :    return;
               :  }
 27303  0.0181 :}
               :
               :// fixes a flow by updating the links on its path
               :static int fix_flow(struct network_t *network, struct flow_t *flow) {
 14242  0.0094 :  for (int i = 0; i < flow->nlinks; i++) {
  2048  0.0014 :    struct link_t *link = flow->links[i];
 27480  0.0182 :    link->used += remaining_demand(flow);
 17229  0.0114 :    if (link->used > link->capacity) {
               :      network_smallest(network);
               :      panic("what the heck mate ...: %d, %d, %.2f, %.2f, %d, %d, %.2f",
               :            link->id, flow->id,
               :            link->used, link->capacity, link->nflows, link->nactive_flows, remaining_demand(flow));
               :    }
               :
 12474  0.0083 :    if (link->nactive_flows==0)
               :      panic("what the heck mate ...: %d, %d, %.2f, %.2f, %d, %d",
               :            link->id, flow->id,
               :            link->used, link->capacity,
               :            link->nflows, network->fixed_flow_end);
               :
  1892  0.0013 :    link->nactive_flows -= 1;
 23682  0.0157 :    recycle_link_if_fixed(network, link);
               :  }
               :
  2090  0.0014 :  flow->fixed = 1;
  2723  0.0018 :  flow->bw = flow->demand;
               :
               :  /* remove the flow from the chain of good flows */
               :  linked_list_remove_flow(flow);
               :  return 1;
               :}
               :
               :// fixes a link by fixing the flows on itself and distributing the spare capacity that it has.
               :static int fix_link(struct network_t *network, struct link_t *link) {
               :  bw_t spare_capacity = per_flow_capacity(link);
               :
               :  struct flow_t *flow = 0;
     1 6.6e-07 :  pair_id_t nflows = link->nflows;
               :
               :  // fix all the flows on the link
     1 6.6e-07 :  for (int i = 0; i < nflows; ++i) {
    32 2.1e-05 :    flow = link->flows[i];
               :    // TODO omida: maybe this shouldn't happen?
   271 1.8e-04 :    if (flow->fixed) continue;
               :
               :    struct link_t *l = 0;
     5 3.3e-06 :    for (int j = 0; j < flow->nlinks; ++j) {
     4 2.6e-06 :      l = flow->links[j];
               :      l->nactive_flows -= 1;
     3 2.0e-06 :      l->used += spare_capacity;
               :
     3 2.0e-06 :      if ((l->used - l->capacity) > EPS) {
               :        network_smallest(network);
               :        panic("what the heck mate ...: %d, %d, %.2f, %.2f, %d, %d, %.2f",
               :              l->id, flow->id,
               :              l->used, l->capacity, l->nflows, l->nactive_flows, remaining_demand(flow));
               :      }
               :
     1 6.6e-07 :      recycle_link_if_fixed(network, l);
               :    }
               :
     1 6.6e-07 :    flow->fixed = 1;
               :    flow->bw += spare_capacity;
               :
               :    /* take it out of the loop */
               :    linked_list_remove_flow(flow);
               :
    39 2.6e-05 :    if (network->smallest_flow == flow) {
               :      network->smallest_flow = flow->next;
               :    }
               :  }
               :
               :  return 1;
               :}
               :
192449  0.1274 :static int flow_cmp(void const *v1, void const *v2) { /* flow_cmp total: 437308  0.2894 */
               :  struct flow_t const *f1 = (struct flow_t const*)v1;
               :  struct flow_t const *f2 = (struct flow_t const*)v2;
               :
 62874  0.0416 :  return (int)(f1->demand - f2->demand);
181985  0.1204 :}
               :
  2349  0.0016 :static int link_cmp_ptr(void const *v1, void const *v2) { /* link_cmp_ptr total:  14617  0.0097 */
   836 5.5e-04 :  struct link_t const *l1 = *(struct link_t const**)v1;
     1 6.6e-07 :  struct link_t const *l2 = *(struct link_t const**)v2;
               :
   804 5.3e-04 :  return (int)((per_flow_capacity(l1)) - (per_flow_capacity(l2)));
   793 5.2e-04 :}
               :
               :static void populate_and_sort_flows(struct network_t *network) {
               :  /* populate the link and flow structures */
     2 1.3e-06 :  link_id_t *ptr = network->routing;
               :  struct flow_t *flow = network->flows;
 25339  0.0168 :  for (int i = 0; i < network->num_flows; ++i, ++flow, ptr += (MAX_PATH_LENGTH + 1)) {
               :    /* if flow has less than EPS demand, just ignore it */
 31606  0.0209 :    if (flow->demand < EPS) {
               :      continue;
               :    }
               :
 69044  0.0457 :    for (int j = 0; j < *ptr; ++j) {
 21082  0.0140 :      struct link_t *link = &network->links[*(ptr+j+1)];
 15077  0.0100 :      flow->links[flow->nlinks++] = link;
 26767  0.0177 :      link->nflows++;
               :    }
               :  }
               :
               :  /* sort the flows by their demand */
    12 7.9e-06 :  qsort(network->flows, network->num_flows, sizeof(struct flow_t), flow_cmp);
               :
    48 3.2e-05 :  flow = network->flows;
  3265  0.0022 :  for (int i = 0; i < network->num_flows; ++i, ++flow)  {
 29374  0.0194 :    if (flow->demand > EPS)
               :      break;
               :  }
    32 2.1e-05 :  network->smallest_flow = flow;
               :}
               :
               :static void populate_and_sort_links(struct network_t *network) {
               :  /* populate the links with non-zero flows */
               :  {
               :    struct flow_t *flow = network->flows;
               :    struct flow_t *prev = 0;
  8845  0.0059 :    for (int i = 0; i < network->num_flows; ++i, ++flow) {
 31451  0.0208 :      if (flow->demand < EPS) {
               :        continue;
               :      }
               :
 29933  0.0198 :      for (int j = 0; j < flow->nlinks; ++j) {
  2671  0.0018 :        struct link_t *link = flow->links[j];
               :        /* create the flows data-structure if it doesn't exist */
 34556  0.0229 :        if (link->flows == 0) {
  2434  0.0016 :          link->flows = malloc(link->nflows * sizeof(struct flow_t *));
               :          link->nactive_flows = 0;
               :        }
               :
 24893  0.0165 :        link->flows[link->nactive_flows++] = flow;
               :      }
               :
               :      // set the proper id for this flow
  1271 8.4e-04 :      flow->id = i;
               :
               :      // properly set the prev and next last flow id
  3492  0.0023 :      flow->prev = prev;
               :
 10761  0.0071 :      if (prev)
  2308  0.0015 :        prev->next = flow;
               :
               :      prev = flow;
               :    }
    30 2.0e-05 :    prev->next = 0;
               :  }
               :
               :
               :  {
               :    /* create sortable links */
    36 2.4e-05 :    struct link_t **links = malloc(sizeof(struct link_t *) * network->num_links);
               :    struct link_t *link = network->links;
    57 3.8e-05 :    for (int i = 0; i < network->num_links; ++i, ++link) {
   163 1.1e-04 :      links[i] = link;
               :    }
    18 1.2e-05 :    qsort(links, network->num_links, sizeof(struct link_t *), link_cmp_ptr);
               :    struct link_t *prev = 0;
               :    network->smallest_link = 0;
               :
    57 3.8e-05 :    for (int i = 0; i < network->num_links; ++i) {
    26 1.7e-05 :      link = links[i];
   482 3.2e-04 :      link->next = link->prev = 0;
   247 1.6e-04 :      if (link->nactive_flows == 0) {
               :        continue;
               :      }
               :
    41 2.7e-05 :      if (prev)
    35 2.3e-05 :        prev->next = link;
    95 6.3e-05 :      link->prev = prev;
               :      prev = link;
               :
   375 2.5e-04 :      if (!network->smallest_link) {
     4 2.6e-06 :        network->smallest_link = link;
               :      }
               :      //printf("building ...\n");
               :    }
               :    prev->next = 0;
    36 2.4e-05 :    free(links);
               :  }
               :}
               :
               :static void network_prepare(struct network_t *network) {
               :  populate_and_sort_flows(network);
               :  populate_and_sort_links(network);
               :}
               :
               :
               :/* calculate the max-min fairness of the network flows. This is a destructive
               :   operation---i.e., the network structure will change */
     5 3.3e-06 :int maxmin(struct network_t *network) { /* maxmin total: 525974  0.3481 */
               :  network_prepare(network);
               :
               :  while (1) {
               :    struct flow_t *flow = find_flow_with_smallest_remaining_demand(network);
               :    struct link_t *link = find_link_with_smallest_remaining_per_flow_bw(network);
               :
  5669  0.0038 :    if (flow == 0 || link == 0)
               :      return 1;
               :
  3183  0.0021 :    if (remaining_demand(flow) < per_flow_capacity(link)) {
  5258  0.0035 :      info("fixing flow: %d", flow->id);
               :      fix_flow(network, flow);
  3464  0.0023 :      network->smallest_flow = flow->next;
               :    } else {
               :      info("> fixing link: %d", link->id);
               :      fix_link(network, link);
               :      info("> done fixing link: %d", link->id);
               :    }
               :  }
     4 2.6e-06 :}
               :
               :int **generate_subplan(int *groups, int groups_len, int *subplan_num)
               :{
               :    int plan_nums = 1;
               :    for (int i = 0; i < groups_len; i++)
               :    {
               :        plan_nums *= groups[i] + 1;
               :    }
               :
               :    *subplan_num = plan_nums;
               :
               :    int **plans = (int **)malloc(sizeof(int *) * plan_nums);
               :    for (int i = 0; i < plan_nums; i++)
               :    {
               :        plans[i] = (int *)malloc(sizeof(int) * groups_len);
               :        //memcpy(plans[i], groups, sizeof(int) * groups_len);
               :    }
               :
               :    for (int i = 0; i < plan_nums; i++)
               :    {
               :        int upgrade_nodes = i;
               :        for (int j = 0; j < groups_len; j++)
               :        {
               :            plans[i][j] = upgrade_nodes % (groups[j] + 1);
               :            upgrade_nodes /= (groups[j] + 1);
               :        }
               :    }
               :
               :    return plans;
               :}
/* 
 * Total samples for file : "/home/jiaqi/planning_config_dic/src/progressive-filling/algorithm.c"
 * 
 * 2122126  1.4045
 */


/* 
 * Command line: opannotate --source --output-dir=./ 
 * 
 * Interpretation of command line:
 * Output annotated source file with samples
 * Output all files
 * 
 * CPU: Intel Broadwell microarchitecture, speed 2900 MHz (estimated)
 * Counted CPU_CLK_UNHALTED events (Clock cycles when not halted) with a unit mask of 0x00 (No unit mask) count 100000
 */
