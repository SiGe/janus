#include <math.h>

#include "config.h"
#include "network.h"
#include "util/log.h"

#include "exec/ltg.h"

#define TO_LTG(e) struct exec_ltg_t *ltg = (struct exec_ltg_t *)(e);

struct mop_t *_ltg_next_mop(
    struct exec_t *exec, struct expr_t *expr) {
  return 0;
}

int _ltg_cmp(void const *v1, void const *v2) {
  struct traffic_stats_t *t1 = (struct traffic_stats_t *)v1;
  struct traffic_stats_t *t2 = (struct traffic_stats_t *)v2;

  if       (t1->in.max < t2->in.max) return -1;
  else if  (t1->in.max > t2->in.max) return  1;

  return 0;
}

int _cp_cmp(void const *v1, void const *v2) {
  struct ltg_critical_path_t *t1 = (struct ltg_critical_path_t *)v1;
  struct ltg_critical_path_t *t2 = (struct ltg_critical_path_t *)v2;

  bw_t b1 = t1->bandwidth / t1->num_switches;
  bw_t b2 = t2->bandwidth / t2->num_switches;

  if       (b1 > b2) return -1;
  else if  (b1 < b2) return  1;

  return 0;
}

static void
_exec_ltg_validate(struct exec_t *exec, struct expr_t const *expr) {
  TO_LTG(exec);

  int subplan_count = 0;
  ltg->trace = traffic_matrix_trace_load(400, expr->traffic_test);
  if (ltg->trace == 0)
    panic("Couldn't load the traffic matrix file: %s", expr->traffic_test);

  ltg->steady_packet_loss = exec_rvar_cache_load(expr, &subplan_count);
  if (ltg->steady_packet_loss == 0)
    panic("Couldn't load the long-term RVAR cache.");

  if (expr->criteria_time == 0)
    panic("Time criteria not set.");

  struct traffic_matrix_trace_iter_t *iter = ltg->trace->iter(ltg->trace);

  /* Get traffic stats for the long-term planner */
  exec_traffic_stats(
      exec, expr, iter, iter->length(iter),
      &(ltg->pod_stats), &(ltg->num_pods), &(ltg->core_stats));

  qsort(ltg->pod_stats, ltg->num_pods, sizeof(struct traffic_stats_t), _ltg_cmp);
  struct ltg_upgrade_plan_t *plan = malloc(sizeof(struct ltg_upgrade_plan_t));
  ltg->plan = plan;

  plan->num_paths = 0;

  uint32_t num_groups = ltg->num_pods + 1 /* core switches */;

  // This is the maximum size of the path
  plan->paths = malloc(sizeof(struct ltg_critical_path_t) * num_groups);
  memset(plan->paths, 0, sizeof(struct ltg_critical_path_t) * num_groups);

  struct ltg_critical_path_t *paths = plan->paths;


  /* The rest of this section builds the critical path component for the
   * upgrade.  The way it works is quite simple.  Get the max bandwidth of each
   * pod and the core switches.  Get the number of switches that we are
   * upgrading in each pod/core.  The critical path would be the path with the
   * max bandwidth/#upgrades */

  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    uint32_t id = ltg->pod_stats[i].id;
    paths[id].bandwidth = ltg->pod_stats[i].in.max;

    // Max number of switches to upgrade
    paths[id].sws = malloc(sizeof(struct jupiter_located_switch_t *) * expr->num_aggs_per_pod);
  }
  paths[num_groups - 1].bandwidth = ltg->core_stats->in.max;
  paths[num_groups - 1].sws = malloc(sizeof(struct jupiter_located_switch_t *) * expr->num_cores);

  for (uint32_t i = 0; i < expr->nlocated_switches; ++i) {
    struct jupiter_located_switch_t *sw = &expr->located_switches[i];
    if (sw->type == AGG) {
      paths[sw->pod].sws[paths[sw->pod].num_switches++] = sw;
    } else if (sw->type == CORE) {
      paths[num_groups - 1].sws[paths[num_groups - 1].num_switches++] = sw;
    } else {
      panic("Unsupported switch type: %d", sw->type);
    }
  }

  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    uint32_t id = ltg->pod_stats[i].id;
    paths[id].bandwidth = ltg->pod_stats[i].in.max;
  }
  paths[num_groups - 1].bandwidth = ltg->core_stats->in.max;
  plan->num_paths = num_groups;
}

static risk_cost_t _exec_ltg_best_plan_at(
    struct exec_t *exec,
    struct expr_t *expr,
    trace_time_t at) {
  // LTG doesn't really care about the best_plan_cost
  // Pack as many subplans as you can 
  TO_LTG(exec);

  struct ltg_upgrade_plan_t *plan = ltg->plan;
  qsort(plan->paths, plan->num_paths, sizeof(struct ltg_critical_path_t), _cp_cmp);


  struct jupiter_located_switch_t **sws = malloc(
      sizeof(struct jupiter_located_switch_t *) * expr->nlocated_switches);


  int nsteps = expr->criteria_time->steps;
  struct mop_t **mops = malloc(
      sizeof(struct mop_t *) * nsteps);

  // TODO: This should somehow use expr->criteria_time->acceptable
  for (uint32_t i = 0; i < nsteps; ++i) {
    int idx = 0;
    for (uint32_t j = 0; j < plan->num_paths; ++j) {
      int nsws = ceil(((double)plan->paths[j].num_switches)/(double)nsteps);
      for (uint32_t k = 0; k < nsws; ++k) {
        sws[idx++] = plan->paths[j].sws[k];
      }
    } 

    // info("Upgrading %d switches for mop %d", i, idx);
    mops[i] = jupiter_mop_for(sws, idx);
  }

  risk_cost_t cost = exec_plan_cost(exec, expr, mops, nsteps, at);
  free(sws);

  // info("Cost of the plan is: %f", cost);
  return cost;
}

static void 
_exec_ltg_runner(struct exec_t *exec, struct expr_t *expr) {
  for (uint32_t i = 50; i < 400; i += 10) {
    trace_time_t at = i;
    risk_cost_t actual_cost = _exec_ltg_best_plan_at(exec, expr, at);
    info("[%4d] Actual cost of the best plan (LTG) is: %4.3f", at, actual_cost);
  }
}

struct exec_t *exec_ltg_create(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_ltg_t));

  exec->validate = _exec_ltg_validate;
  exec->run = _exec_ltg_runner;

  return exec;
}
