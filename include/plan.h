#ifndef _PLAN_H_
#define _PLAN_H_

#include <stdint.h>

#include "khash.h"

struct network_t;
typedef uint32_t mop_steps_t;

struct plan_t {
  struct plan_iterator_t* (*iter) (struct plan_t);
};

struct mop_t {
  /* Apply a pre mop on the network */
  int             (*pre)        (struct mop_t *, struct network_t*);

  /* Apply a post mop on the network */
  int             (*post)       (struct mop_t *, struct network_t*);

  /* Number of steps that we should run this operation */
  mop_steps_t (*operation)      (struct mop_t *, struct network_t*);
};

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

struct jupiter_located_switch_t {
  uint32_t                 sid;
  enum JUPITER_SWITCH_TYPE type;
  uint16_t                 color;  // For traffic similarity
  uint16_t                 pod;    // Pod number for this switch
};

struct jupiter_switch_upgrade_planner_t {
  struct plan_t;

  uint32_t num_switches;
  struct jupiter_located_switch_t *switches;
};

struct jupiter_group_t {
  struct jupiter_located_switch_t **switches;
  uint32_t num_switches, cap;
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
