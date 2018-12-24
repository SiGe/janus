#ifndef _JUPITER_H_
#define _JUPITER_H_

#include "dataplane.h"
#include "network.h"

enum SWITCH_STAT {
  DOWN,
  UP,
};

struct switch_stats_t {
  enum SWITCH_STAT stat;
  switch_id_t      id;
};

struct jupiter_network_t {
  struct network_t;
  struct clone_t;

  uint32_t core, agg, pod, tor;
  bw_t     link_bw;
  struct switch_stats_t *switches;

  struct switch_stats_t *core_ptr;
  struct switch_stats_t *agg_ptr;
  struct switch_stats_t *tor_ptr;

  char end[];
};

/* Creates a new jupiter network */
struct network_t *
jupiter_network_create(
    uint32_t core,
    uint32_t pod,
    uint32_t agg_per_pod,
    uint32_t tor_per_pod,
    bw_t link_bw);


int  jupiter_apply_mops (struct network_t *, struct mop_t*);
int  jupiter_step (struct network_t *);
int  jupiter_set_traffic (struct network_t *, struct traffic_matrix_t const*);
int  jupiter_get_traffic (struct network_t *, struct traffic_matrix_t const**);
int  jupiter_get_dataplane (struct network_t *, struct dataplane_t*); 
void jupiter_drain_switch(struct network_t *, switch_id_t);
void jupiter_undrain_switch(struct network_t *, switch_id_t);
void jupiter_network_free(struct jupiter_network_t *);

switch_id_t jupiter_get_core(struct network_t *, uint32_t);
switch_id_t jupiter_get_agg(struct network_t *, uint32_t, uint32_t);

#endif // _JUPITER_H_
