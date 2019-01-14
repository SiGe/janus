#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "util/log.h"
#include "plan.h"

struct jupiter_switch_upgrade_planner_t *jupiter_switch_upgrade_planner_new(
    uint32_t num_switches, struct jupiter_located_switch_t const *switches) {
  if (num_switches == 0)
    panic("Creating a planner with no switches ...");

  size_t size = sizeof(struct jupiter_switch_upgrade_planner_t);
  struct jupiter_switch_upgrade_planner_t *planner= malloc(size);
  planner->num_switches = num_switches;
  size = sizeof(struct jupiter_located_switch_t) * num_switches;
  planner->switches = malloc(size);
  memcpy(planner->switches, switches, size);

  return planner;
}

void jupiter_switch_upgrade_planner_free(
    struct jupiter_switch_upgrade_planner_t *jup) {
  if (jup->num_switches > 0) {
    free(jup->switches);
    jup->num_switches = 0;
    jup->switches = 0;
  }
  free(jup);
}

#define TO_JITER(p) struct jupiter_switch_upgrade_plan_iterator_t *jiter =\
                      ((struct jupiter_switch_upgrade_plan_iterator_t *)(p))

#define JUPITER_DEFAULT_GROUP_SIZE 10

void _group_insert(khash_t(jupiter_groups) *h, 
    struct jupiter_located_switch_t *sw) {
  uint16_t color = sw->color;
  uint16_t pod = sw->pod;
  enum JUPITER_SWITCH_TYPE type = sw->type;

  uint64_t id = (((((uint64_t)pod) << 16) + (uint64_t)(color)) << 16) + (uint64_t)type;
  khiter_t iter = kh_get(jupiter_groups, h, id);
  struct jupiter_group_t *group = 0;

  if (iter == kh_end(h)) {
    group = malloc(sizeof(struct jupiter_group_t));
    group->switches = malloc(sizeof(uintptr_t) * JUPITER_DEFAULT_GROUP_SIZE);
    group->num_switches = 0;
    group->cap = JUPITER_DEFAULT_GROUP_SIZE;
  } else {
    group = kh_value(h, iter);
  }

  if (group->cap == group->num_switches) {
    group->cap *= 2;
    group->switches = realloc(group->switches, group->cap * sizeof(uintptr_t));
  }

  group->switches[group->num_switches++] = sw;
}


void _sup_begin (struct plan_iterator_t *iter) {
  TO_JITER(iter);
  (void)jiter;
}

int  _sup_next (struct plan_iterator_t *iter, struct mop_t **mop, int *num_ops) {
  return 0;
}

int  _sup_end (struct plan_iterator_t *iter) {
  return 0;
}

void _sup_free(struct plan_iterator_t *iter) {
  TO_JITER(iter);
  // Free the iterator
  for (uint32_t i = 0; i < jiter->num_groups; ++i) {
    free(jiter->groups[i]->switches);
    free(jiter->groups[i]);
  }

  free(jiter->groups);
  free(jiter);
}

struct jupiter_switch_upgrade_plan_iterator_t *_sup_init(
    struct jupiter_switch_upgrade_planner_t *planner) {
  struct jupiter_switch_upgrade_plan_iterator_t *iter = malloc(
      sizeof(struct jupiter_switch_upgrade_plan_iterator_t));

  iter->num_switches = planner->num_switches;
  size_t size = sizeof(struct jupiter_located_switch_t) * iter->num_switches;
  iter->switches = malloc(size);
  memcpy(iter->switches, planner->switches, size);

  iter->begin = _sup_begin;
  iter->end   = _sup_end;
  iter->next  = _sup_next;
  iter->free  = _sup_free;
  iter->granularity = iter->granularity;

  /* Setup the groups */
  khash_t(jupiter_groups) *h = kh_init(jupiter_groups);
  for (uint32_t i = 0; i < iter->num_switches; ++i) {
    _group_insert(h, &iter->switches[i]);
  }

  iter->num_groups = kh_size(h);
  iter->groups = malloc(sizeof(struct jupiter_group_t *) * iter->num_groups);

  uint64_t index = 0, kvar = 0;
  struct jupiter_group_t *group = 0;

  kh_foreach(h, kvar, group, {
      (void)(kvar);
      iter->groups[index] = group;
      index++;
  });

  // Free the memory
  kh_destroy(jupiter_groups, h);

  return iter;
}

struct jupiter_switch_upgrade_plan_iterator_t *
jupiter_switch_upgrade_planner_iterator(
    struct jupiter_switch_upgrade_planner_t *planner) {
  return _sup_init(planner);
}