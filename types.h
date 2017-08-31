#ifndef _TYPES_H_
#define _TYPES_H_

#include <stdint.h>

/* Max path length for each flow */
#define MAX_PATH_LENGTH 4

/* link and pair ids */
typedef uint16_t link_id_t;
typedef uint16_t pair_id_t;

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

struct tm_t {
  bw_t *tm;
};

struct traffic_t {
  int tm_num;
  
  /* The traffics vector's order is consistent with flow/routing */
  struct tm_t *tms;
};

/* Network representation */
struct network_t {
  link_id_t *routing;
  struct link_t *links;
  struct flow_t *flows;

  link_id_t num_links;
  pair_id_t num_flows;

  int fixed_flow_end;
  pair_id_t *flow_ids;

  int fixed_link_end;
  link_id_t *link_ids;

  struct flow_t *smallest_flow;
  struct link_t *smallest_link;

  int k;
  int t_per_p;
  int a_per_p;
  int c_num;
};

struct error_t
{
    int tot_samples;
    int predict_num;
    int sd_pair_num;
    bw_t ***error_tms;
};

#endif
