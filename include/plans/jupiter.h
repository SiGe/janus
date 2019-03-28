#ifndef _PLANS_JUPITER_H_
#define _PLANS_JUPITER_H_

#include <stdint.h>
#include "plan.h"

/* Jupiter network MOP planner  */
/* TODO: Debatable whether I should merge MOp with switch_id_t methods in
 * networks/jupiter.h.
 *
 * - Omid 3/29/2019 */
enum JUPITER_SWITCH_TYPE {
  JST_CORE, JST_AGG, JST_TOR
};

/* Properties that we care about in a jupiter topology for iterating through the plans are:
 * The color, the pod, and the type of the switch. */
struct jupiter_located_switch_t {
  uint32_t                 sid;    /* Switch ID ... this is MOP specific and
                                      does not map to switch_id_t in
                                      networks/jupiter.h */
  enum JUPITER_SWITCH_TYPE type;   /* Type of the switch */
  uint16_t                 color;  /* Color of the switch---this should be
                                      assigned based on the routing algorithm
                                      and traffic volume (at the edge, i.e.,
                                      ToRs) */
  uint16_t                 pod;    /* Pod number for this switch */
};

struct jupiter_class_t {
  struct jupiter_located_switch_t **switches; /* A class is a list of switches
                                                 under the same freedom degree
                                                 */

  /* TODO: This seems redundant---probably needs a fwd/back pointer  to/from
   * the switches*/
  uint16_t pod;                  /* The pod this class belongs to */
  enum JUPITER_SWITCH_TYPE type; /* Type of the switches in this class */
  uint32_t nswitches;            /* Number of switches in the located_switch_t array */
};

/* A group is a set of switch classes that get upgraded together.  At each step
 * of the group, we upgrade 1/group_size from each class.  We take the ceil to
 * be conservative.  But in general, we may want to deal with remainders (makes
 * planning much more complex) */
struct jupiter_group_t {
  uint32_t group_size;          /* How many "steps" this group can take for the upgrade */
  uint32_t nclasses;            /* Number of classes in this group */
  struct   jupiter_class_t *classes; /* List of the classes */
};

struct jupiter_multigroup_t {
  struct jupiter_group_t *groups; /* List of groups */
  uint32_t ngroups;               /* Number of groups --- i.e., number of
                                     freedom degrees in the ini file */
};

KHASH_MAP_INIT_INT(jupiter_groups, struct jupiter_group_t*)

/* Jupiter upgrade planner groups the switches into "granularity" groups. */
struct jupiter_switch_plan_enumerator_t {
  struct plan_t;

  uint32_t num_switches;
  struct jupiter_located_switch_t *switches;
  uint32_t granularity;

  /* Jupiter is a special topology and building the planner group is very
   * straightforward: 
   *
   * 1) We first group the switches with the same color/pod/type and create a multiset
   * 2) Then we build an iterator that iterates through all the partitions of a set.
   *
   * In general, when dealing with more complex topologies, we would want to use
   * nauty and build the degree of freedom, classes, and multigroups
   * automatically.  Ideally, "blocks" would be the building blocks where a
   * "block" defines a group of nodes which are topologically/and routing wise
   * indifferent.
   * */
  struct jupiter_multigroup_t multigroup;
};

struct jupiter_switch_plan_enumerator_iterator_t {
  struct plan_iterator_t;   /* A jupiter switch plan enumerator iterator is a
                               "plan" iterator */

  /* Use the switch groups as the basis of iteration algorithm */
  struct jupiter_switch_plan_enumerator_t const *planner;

  /* Refer to group_gen.h, the state is basically a group iter for the
   * jupiter_group_t's in the planner.  This is also known as the freedom
   * degrees of the planner. */
  struct group_iter_t *state;

  /* Eh, mallocing and freeing the tuple space for the iterator is too expensive
   * (compared to the operations done on the tuple) might as well build it once. */
  uint32_t *_tuple_tmp;
};

/* Creates a plan enumerator with the freedom degrees specified */
struct jupiter_switch_plan_enumerator_t *jupiter_switch_plan_enumerator_create(
    uint32_t num_switches, struct jupiter_located_switch_t const *switches, 
    uint32_t *freedom_degree, uint32_t ndegree);

/* Free the plan enumerator */
void jupiter_switch_plan_enumerator_free(
    struct jupiter_switch_plan_enumerator_t *);

/* Returns the mop for a subplan id */
struct mop_t *jupiter_mop_for(
    struct jupiter_located_switch_t **, uint32_t);

#endif //  _PLAN_JUPITER_H_

