#ifndef _JUPITER_H_
#define _JUPITER_H_

#include "dataplane.h"
#include "network.h"

/* Jupiter network switch stats
 *
 * Mainly used for creation of dataplane.  If a switch is DOWN it does not
 * participate in the capacity estimation of the dataplane.  Whereas UP does
 * otherwise.
 * */
enum SWITCH_STAT {
  DOWN,
  UP,
};

/* Status of a switch.  Each switch ID uniquely identifies the switch
 * type/location/ and purpose in the jupiter topology.  For that, we have to
 * rely on core/agg/tor and pod numbers in the jupiter_network_t structure */
struct switch_stats_t {
  enum SWITCH_STAT stat; /* Status of the switch */
  switch_id_t      id;   /* Switch identifier */
};

struct jupiter_network_t {
  struct network_t;  /* A jupiter network is a ... network */

  uint32_t core, agg, pod, tor;    /* Topology specific parameters for Jupiter network */
  bw_t     link_bw;                /* Only one link-bandwidth exists across the whole topology */
  struct switch_stats_t *switches; /* List of switches in the topology */

  /* TODO: Can possible replace this with accessor functions? OR macros
   *
   * - Omid 3/26/2019 */
  struct switch_stats_t *core_ptr; /* Pointers to core switches */
  struct switch_stats_t *agg_ptr;  /* Pointers to aggregate switches */
  struct switch_stats_t *tor_ptr;  /* Pointers to tor switches */

  /* TODO: I used to like putting data at the end of the structure.  Probably
   * not the best idea here.
   *
   * - Omid 3/26/2019 */
  char end[];
};

/* Creates a new jupiter network with the passed parameters. */
struct network_t *
jupiter_network_create(
    uint32_t core,
    uint32_t pod,
    uint32_t agg_per_pod,
    uint32_t tor_per_pod,
    bw_t link_bw);


/* Setup a traffic matrix on the jupiter topology.  Mainly used for dataplane creation */
int  jupiter_set_traffic (struct network_t *, struct traffic_matrix_t const*);

/* Returns the active traffic matrix on the topology */
int  jupiter_get_traffic (struct network_t *, struct traffic_matrix_t const**);

/* Builds and returns the current dataplane */
int  jupiter_get_dataplane (struct network_t *, struct dataplane_t*); 

/* Drains a switch, i.e., it sets the status of the switch to "DOWN".  Uses the
 * switch_id, which can be obtained by leverating the jupiter_get_core/agg
 * helper functions. ToRs are irrelevant at this stage so there is no helper
 * function for them.*/
void jupiter_drain_switch(struct network_t *, switch_id_t);

/* Undrains a switch, i.e., it sets the status of the switch to "UP".  Uses the
 * switch_id, which can be obtained by leverating the jupiter_get_core/agg
 * helper functions. ToRs are irrelevant at this stage so there is no helper
 * function for them.*/
void jupiter_undrain_switch(struct network_t *, switch_id_t);
void jupiter_network_free(struct network_t *);

/* Returns the switch id of the ith core switch */
switch_id_t jupiter_get_core(struct network_t *, uint32_t);

/* Returns the switch id of the ith agg switch in the jth pod */
switch_id_t jupiter_get_agg(struct network_t *, uint32_t, uint32_t);

#endif // _JUPITER_H_
