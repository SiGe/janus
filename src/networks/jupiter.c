#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dataplane.h"
#include "networks/jupiter.h"
#include "util/log.h"

#define TO_J(name) struct jupiter_network_t *jup = (struct jupiter_network_t *)(name)
#define MAX_PODS 256

inline static uint32_t _num_switches(
    uint32_t core, uint32_t pod, 
    uint32_t agg, uint32_t tor) {
  return core + pod * (agg + tor);
}

inline static uint32_t _num_switches_jupiter(
    struct jupiter_network_t *jupiter) {
  return _num_switches(jupiter->core, jupiter->pod, jupiter->agg, jupiter->tor);
}

inline static uint32_t _num_active_cores_jupiter(
    struct jupiter_network_t *jup) {
  uint32_t active_cores = 0;
  for (struct switch_stats_t *core = jup->core_ptr; core != jup->agg_ptr; ++core) {
    if (core->stat == UP) {
      active_cores += 1;
    }
  }
  return active_cores;
};

inline static __attribute__((unused))
uint32_t _tor_to_pod(struct jupiter_network_t *jup, uint32_t tor) {
  return (tor / jup->tor);
}

inline static __attribute__((unused)) 
uint32_t _agg_to_pod(struct jupiter_network_t *jup, uint32_t agg) {
  return (agg / jup->agg);
}

inline static uint32_t _num_active_aggs_jupiter(
    struct jupiter_network_t *jup, uint32_t pod) {
  uint32_t active_aggs = 0;
  uint32_t agg_count = jup->agg;
  struct switch_stats_t *base = jup->agg_ptr + pod * agg_count;
  for (struct switch_stats_t *agg = base; agg != base + agg_count; ++agg) {
    if (agg->stat == UP) {
      active_aggs += 1;
    }
  }
  return active_aggs;
};

inline static void _setup_routing_for_pair(
    struct jupiter_network_t *jup,
    uint32_t sid, uint32_t did, link_id_t *links) {
    uint32_t spod = _tor_to_pod(jup, sid);
    uint32_t dpod = _tor_to_pod(jup, did);
    uint32_t num_core_links = jup->pod * 2;
    //uint32_t num_pod_links = jup->tor * 2;
    //

    if (spod == dpod) {
      *links++ = 2;
      *links++ = sid * 2 + num_core_links + 0 /* upward */;
      *links++ = did * 2 + num_core_links + 1 /* downward */;
    } else {
      *links++ = 4;
      *links++ = sid * 2 + num_core_links + 0 /* upward */;
      *links++ = 2 * spod + 0 /* upward to cores */;
      *links++ = 2 * dpod + 1 /* downward to aggs */;
      *links++ = did * 2 + num_core_links + 1 /* downward */;
    }
}

uint32_t static _setup_bandwidth_for_links(
    struct jupiter_network_t *jup, struct link_t **out) {
  uint32_t active_aggs_per_pod[MAX_PODS];
  for (uint32_t pod = 0; pod < jup->pod; ++pod) {
    active_aggs_per_pod[pod] = _num_active_aggs_jupiter(jup, pod);
  }
  uint32_t active_cores = _num_active_cores_jupiter(jup);
  size_t size = (jup->pod * 2 + jup->pod * jup->tor * 2)  * sizeof(struct link_t);
  struct link_t *links = malloc(size);
  memset(links, 0, size);
  *out = links;

  link_id_t id = 0;
  for (uint32_t pod = 0; pod < jup->pod; ++pod) {
    /* Up */
    links->capacity = jup->link_bw * active_cores * active_aggs_per_pod[pod];
    links->used = 0; links->id = id++; links->nflows = 0;
    links++;
    /* Down */
    links->capacity = jup->link_bw * active_cores * active_aggs_per_pod[pod];
    links->used = 0; links->id = id++; links->nflows = 0;
    links++;
  }

  for (uint32_t pod = 0; pod < jup->pod; ++pod) {
    for (uint32_t tor = 0; tor < jup->tor; ++tor) {
      links->capacity = jup->link_bw * active_aggs_per_pod[pod];
      links->used = 0; links->id = id++; links->nflows = 0;
      links++;
      links->capacity = jup->link_bw * active_aggs_per_pod[pod];
      links->used = 0; links->id = id++; links->nflows = 0;
      links++;
    }
  }

  return (jup->pod * 2 + jup->pod * jup->tor * 2);
}

uint32_t static _setup_bandwidth_for_flows(
    struct jupiter_network_t *jup, struct flow_t **out) {
  /* Init the flows */
  struct flow_t *flows = 0;
  size_t size = sizeof(struct flow_t) * jup->tm->num_pairs;
  size_t num_flows = 0;
  struct pair_bw_t const *pair = jup->tm->bws;

  pair_id_t num_tors = jup->tor * jup->pod;

  flows = malloc(size);
  memset(flows, 0, size);
  *out = flows;
  for (uint32_t i = 0; i < jup->tm->num_pairs; ++i) {
    // No need to setup a flow if bw is zero
    if (pair->bw == 0) {
      pair++;
      continue;
    }

    /* Setup the flow */
    flows->demand = pair->bw;
    flows->bw = 0; flows->nlinks = 0;
    flows->stor = i / num_tors;
    flows->dtor = i % num_tors;

    /* Move the iterators */
    flows++; num_flows++; pair++;
  }
  return num_flows;
}


struct network_t *
jupiter_network_create(
    uint32_t core,
    uint32_t pod,
    uint32_t agg_per_pod,
    uint32_t tor_per_pod,
    bw_t link_bw) {

  /* Allocate space for our network */
  size_t mem_size = \
    sizeof(struct switch_stats_t) * _num_switches(core, pod, agg_per_pod, tor_per_pod) +\
    sizeof(struct jupiter_network_t);
  struct jupiter_network_t *ret = malloc(mem_size);

  /* Set variables */
  ret->switches = (struct switch_stats_t *)ret->end;
  ret->core_ptr = ret->switches;
  ret->agg_ptr  = ret->core_ptr + core;
  ret->tor_ptr  = ret->agg_ptr + agg_per_pod * pod;
  ret->core     = core;
  ret->pod      = pod;
  ret->agg      = agg_per_pod;
  ret->tor      = tor_per_pod;
  ret->link_bw  = link_bw;

  /* Set the functions */
  ret->apply           = jupiter_apply_mops;
  ret->step            = jupiter_step;
  ret->set_traffic     = jupiter_set_traffic;
  ret->get_traffic     = jupiter_get_traffic;
  ret->get_dataplane   = jupiter_get_dataplane;
  ret->drain_switch    = jupiter_drain_switch;;
  ret->undrain_switch  = jupiter_undrain_switch;
  ret->free            = jupiter_network_free;

  ret->clone   = 0;
  ret->save    = 0;
  ret->restore = 0;

  /* Initialize the data-structures */
  for (switch_id_t i = 0; i < _num_switches_jupiter(ret); ++i) {
    ret->switches[i].stat = UP;
    ret->switches[i].id   = i;
  }

  /* Set functions */
  return (struct network_t *)ret;
}

int jupiter_apply_mops(struct network_t *net, struct mop_t *mop) {
  TO_J(net);
  (void)(jup);

  return 0;
}

int jupiter_step(struct network_t *net) {
  TO_J(net);
  (void)(jup);

  return 0;
};

void jupiter_drain_switch(struct network_t *net, switch_id_t id) {
  TO_J(net);
  jup->switches[id].stat = DOWN;
};

void jupiter_undrain_switch(struct network_t *net, switch_id_t id) {
  TO_J(net);
  jup->switches[id].stat = UP;
};

int jupiter_get_dataplane(struct network_t *net, struct dataplane_t *dp) {
  /* A logical network for a jupiter network is very simple: a single core
   * switch, a single switch per pod, which come with "fat links", each ToR
   * should be considered seperately though.
   *
   * For the first step, we calculate the "bandwidth" between different layers
   * as follows:
   *
   * 1) ToR to Agg:  #aggs * bw_t upward
   * 2) Agg to ToR:  #aggs * bw_t downward
   * 3) Agg to Core: #aggs * #cores * bw_t upward: Each agg is connected to
   *                 all the cores and we have many physical aggs and only a
   *                 single logical agg.  It's like a 1:#tor spraying.
   * 4) Core to Agg: #aggs * #cores * bw_t downward: Similar to above.
   */

  TO_J(net);
  
  /* Initialize dataplane */
  dataplane_init(dp);

  /*
   * We only need to set the routing, links, and flows:
   *
   * For links:
   *  id, capacity, used=0 should be set.  Do not need to set any other attributes.
   *
   * For flows:
   *  id, demand, bw=0 should be set.
   *
   * For routing:
   *  Each path is of length MAX_PATH_LENGTH + 1:
   *    First entry is the length of the path
   *    Second and unward (until MAX_PATH_LENGTH + 1) are the link_ids
   *  E.g., 2, 10, 2, 0, 0 Means that we have path length of 2 going
   *  through links 10 and 2 for a MAX_PATH_LENGTH of 4.
   */

  /* setup the routing */
  link_id_t      *routing = malloc(sizeof(link_id_t) * jup->tm->num_pairs * (MAX_PATH_LENGTH + 1));
  link_id_t      *ptr = routing;
  struct pair_bw_t const *pair = jup->tm->bws;
  pair_id_t num_tors = jup->tor * jup->pod;
  assert(jup->tm->num_pairs == num_tors * num_tors);

  for (pair_id_t s = 0; s < num_tors; ++s) {
    for (pair_id_t d = 0; d < num_tors; ++d) {
      // No need to setup pair routing if there is no traffic.
      if (pair->bw == 0) {
        pair++; continue;
      }

      _setup_routing_for_pair(jup, s, d, ptr);
      ptr += (MAX_PATH_LENGTH + 1);
      pair++;
    }
  }

  struct flow_t *flows = 0;
  struct link_t *links = 0;

  uint32_t num_links = _setup_bandwidth_for_links(jup, &links);
  uint32_t num_flows = _setup_bandwidth_for_flows(jup, &flows);

  (dp)->num_flows = num_flows;
  (dp)->num_links = num_links;
  (dp)->links = links;
  (dp)->flows = flows;
  (dp)->routing = routing;

  return 0;
};

int jupiter_set_traffic (struct network_t *net, struct traffic_matrix_t const *tm) {
  TO_J(net);
  jup->tm = tm;
  return 0;
}

int jupiter_get_traffic (struct network_t *net, struct traffic_matrix_t const **tm) {
  TO_J(net);
  *tm = jup->tm;

  return 0;
}


void jupiter_network_free(struct network_t *net) {
  free(net);
}

switch_id_t jupiter_get_core(struct network_t *net, uint32_t num) {
  TO_J(net);
  assert(num < jup->core);
  return num;
}

switch_id_t jupiter_get_agg(struct network_t *net, uint32_t pod, uint32_t num) {
  TO_J(net);
  assert(pod < jup->pod);
  assert(num < jup->agg);
  return pod * jup->agg + num + jup->core;
}
