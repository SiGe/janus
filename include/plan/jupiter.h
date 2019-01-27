#ifndef _PLAN_JUPITER_H_
#define _PLAN_JUPITER_H_

#include <stdint.h>
#include "plan.h"

/* Jupiter network MOP planner  */
enum JUPITER_SWITCH_TYPE {
  CORE, AGG, TOR
};

/* Properties that we care about in a jupiter topology for iterating through the plans are:
 * The color, the pod, and the type of the switch.
 */
struct jupiter_located_switch_t {
  uint32_t                 sid;
  enum JUPITER_SWITCH_TYPE type;
  uint16_t                 color;  // For traffic similarity
  uint16_t                 pod;    // Pod number for this switch
};

struct jupiter_class_t {
  struct jupiter_located_switch_t **switches;
  uint16_t color;
  uint16_t pod;
  enum JUPITER_SWITCH_TYPE type;
  uint32_t nswitches;
};

struct jupiter_group_t {
  uint32_t group_size;
  uint32_t nclasses;
  struct   jupiter_class_t *classes;
};

struct jupiter_multigroup_t {
  struct jupiter_group_t *groups;
  uint32_t ngroups;
};

KHASH_MAP_INIT_INT(jupiter_groups, struct jupiter_group_t*);

/* Jupiter upgrade planner groups the switches into "granularity" groups. */
struct jupiter_switch_plan_enumerator_t {
  struct plan_t;

  uint32_t num_switches;
  struct jupiter_located_switch_t *switches;
  uint32_t granularity;

  // Jupiter is a special topology and building the planner group is very
  // straightforward: 
  //
  // 1) We first group the switches with the same color/pod/type and create a multiset
  // 2) Then we build an iterator that iterates through all the partitions of a set.
  struct jupiter_multigroup_t multigroup;
};

struct jupiter_switch_plan_enumerator_iterator_t {
  struct plan_iterator_t;

  struct jupiter_switch_plan_enumerator_t const *planner;

  // We use this variable to keep track of where we are in the iteration.
  struct group_iter_t *state;
  uint32_t *_tuple_tmp;
};

struct jupiter_switch_plan_enumerator_t *jupiter_switch_plan_enumerator_create(
    uint32_t num_switches, struct jupiter_located_switch_t const *switches, 
    uint32_t *freedom_degree, uint32_t ndegree);

void jupiter_switch_plan_enumerator_free(
    struct jupiter_switch_plan_enumerator_t *);

struct plan_iterator_t*
jupiter_switch_plan_enumerator_iterator(struct plan_t *planner);

struct mop_t *jupiter_mop_for(
    struct jupiter_located_switch_t **, uint32_t);

#endif //  _PLAN_JUPITER_H_

