#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

/* Max path length for each flow */
#define MAX_PATH_LENGTH 4

/* link and pair ids */
typedef uint32_t link_id_t;
typedef uint32_t pair_id_t;

/* bandwidth type variable */
typedef double bw_t;

struct link_t {
  link_id_t id;

  pair_id_t nactive_flows;
  pair_id_t nflows;
  bw_t capacity;
  bw_t used;

  struct flow_t **flows;

  /* next and prev point in the linked list */
  struct link_t *next, *prev;
};

struct flow_t {
  pair_id_t id;

  uint8_t fixed;
  uint8_t nlinks;

  bw_t bw;
  bw_t demand;

  struct link_t *links[MAX_PATH_LENGTH];

  /* next and prev point in the linked list */
  struct flow_t *next, *prev;
};

/* Dataplane representation */
struct dataplane_t {
  /* Structure holding the links per flow.  This is a 2D structure where routing
   * for the i_th flow is available from:
   *    base = i_th * (MAX_PATH_LENGHT + 1)
   *    [base ...  base + MAX_PATH_LENGTH]
   *    First entry is the number of links on the path
   *    Second and onward are the links on the path
   */
  link_id_t *routing;

  /* List of links in the network */
  struct link_t *links;

  /* List of flows in the network */
  struct flow_t *flows;

  /* Number of links in the network */
  link_id_t num_links;

  /* Number of flows in the network */
  pair_id_t num_flows;

  /* XXX: Useless? */
  int fixed_flow_end;
  pair_id_t *flow_ids;

  /* XXX: Useless? */
  int fixed_link_end;
  link_id_t *link_ids;

  struct flow_t *smallest_flow;
  struct link_t *smallest_link;
};

void dataplane_init(struct dataplane_t *);

#endif
