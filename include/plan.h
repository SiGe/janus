#ifndef _PLAN_H_
#define _PLAN_H_

#include <stdint.h>

#include "khash.h"

struct network_t;
typedef uint32_t mop_steps_t;

/* A management operation has three functions: pre, operation, post.
 *
 * The pre function prepares the network for the management operation.  
 * The operation function telling us the duration that network remain in pre.
 * The post function rejoins the elements back to the network.
 */
struct mop_t {
  /* Apply a pre mop on the network */
  int             (*pre)        (struct mop_t *, struct network_t*);

  /* Apply a post mop on the network */
  int             (*post)       (struct mop_t *, struct network_t*);

  /* Number of steps that we should run this operation */
  mop_steps_t (*operation)      (struct mop_t *, struct network_t*);
};

/* A planner has one interface ... a plan iterator that lets us iterate
 * throught the feasible set of plans.  
 *
 * Possibly each network would want to specialize the iterator (or we can write
 * a generic iterator that uses nauty in the background and does the prunin
 * automatically but that takes too much time ... for this paper).
 */
struct plan_t {
  struct plan_iterator_t* (*iter) (struct plan_t);
};

/* Iterator that returns an ordered set of mops for a specific step of a plan.
 *
 * What we (probably?) want to do is to just iterate through all the subsets and
 * for the second step return the PLAN - the subset so that the long-term
 * planner can find the best plan.
 *
 * The first step is not commutative but the long-term plan IS commutative.  So
 * the query to the long-term planner would be: 
 *
 *    Give me the best set of steps to finish a plan under XX budget.
 *
 * And the long term planner would do a query to its internal database to find
 * that plan.
 */
struct plan_iterator_t {
  void (*begin) (struct plan_iterator_t *);
  void (*free)  (struct plan_iterator_t *);
  int  (*next)  (struct plan_iterator_t *, struct mop_t **, int *);
  int  (*end)   (struct plan_iterator_t *);
};

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

/* Jupiter upgrade planner groups the switches into "granularity" groups. */
struct jupiter_switch_upgrade_planner_t {
  struct plan_t;

  uint32_t num_switches;
  struct jupiter_located_switch_t *switches;
  uint32_t granularity;
};

struct jupiter_group_t {
  struct jupiter_located_switch_t **switches;
  uint32_t num_switches, cap;


  // We use this variable to keep track of where we are in the iteration.
  struct _group_iter_state *state;
};

KHASH_MAP_INIT_INT(jupiter_groups, struct jupiter_group_t*);

struct jupiter_switch_upgrade_plan_iterator_t {
  struct plan_iterator_t;

  uint32_t num_switches;
  uint32_t granularity;

  struct jupiter_located_switch_t *switches;
  struct jupiter_switch_upgrade_planner_t const *planner;

  // Jupiter is a special topology to build the planner where building the
  // planner group is very straightforward: 
  //
  // 1) We first group the switches with the same color/pod/type
  // 2) Then we build an iterator that iterates through all the partitions of a set.
  uint32_t num_groups;
  struct jupiter_group_t **groups;
};

struct jupiter_switch_upgrade_planner_t *jupiter_switch_upgrade_planner_new(
    uint32_t num_switches, struct jupiter_located_switch_t const *switches);

void jupiter_switch_upgrade_planner_free(
    struct jupiter_switch_upgrade_planner_t *);

#endif // _PLAN_H_
