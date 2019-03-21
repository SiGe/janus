#include <math.h>

#include "config.h"
#include "network.h"
#include "util/log.h"

#include "exec/stg.h"

#define TO_STG(e) struct exec_stg_t *stg = (struct exec_stg_t *)(e);

struct mop_t *_stg_next_mop(
    struct exec_t *exec, struct expr_t *expr) {
  return 0;
}

static int _stg_cmp(void const *v1, void const *v2) {
  struct traffic_stats_t *t1 = (struct traffic_stats_t *)v1;
  struct traffic_stats_t *t2 = (struct traffic_stats_t *)v2;

  if       (t1->in.max < t2->in.max) return -1;
  else if  (t1->in.max > t2->in.max) return  1;

  return 0;
}

static int _cp_cmp(void const *v1, void const *v2) {
  struct stg_critical_path_t *t1 = (struct stg_critical_path_t *)v1;
  struct stg_critical_path_t *t2 = (struct stg_critical_path_t *)v2;

  bw_t b1 = t1->bandwidth / t1->num_switches;
  bw_t b2 = t2->bandwidth / t2->num_switches;

  if       (b1 > b2) return -1;
  else if  (b1 < b2) return  1;

  return 0;
}

static void
_exec_stg_validate(struct exec_t *exec, struct expr_t const *expr) {
  TO_STG(exec);

  unsigned subplan_count = 0;
  stg->trace = traffic_matrix_trace_load(400, expr->traffic_test);
  if (stg->trace == 0)
    panic("Couldn't load the traffic matrix file: %s", expr->traffic_test);

  stg->steady_packet_loss = exec_rvar_cache_load(expr, &subplan_count);
  if (stg->steady_packet_loss == 0)
    panic_txt("Couldn't load the long-term RVAR cache.");

  if (expr->criteria_time == 0)
    panic_txt("Time criteria not set.");

  struct traffic_matrix_trace_iter_t *iter = stg->trace->iter(stg->trace);

  /* Get traffic stats for the long-term planner */
  exec_traffic_stats(
      exec, expr, iter, iter->length(iter),
      &(stg->pod_stats), &(stg->num_pods), &(stg->core_stats));

  qsort(stg->pod_stats, stg->num_pods, sizeof(struct traffic_stats_t), _stg_cmp);
  struct stg_upgrade_plan_t *plan = malloc(sizeof(struct stg_upgrade_plan_t));
  stg->plan = plan;

  plan->num_paths = 0;

  uint32_t num_groups = stg->num_pods + 1 /* core switches */;

  // This is the maximum size of the path
  plan->paths = malloc(sizeof(struct stg_critical_path_t) * num_groups);
  memset(plan->paths, 0, sizeof(struct stg_critical_path_t) * num_groups);

  struct stg_critical_path_t *paths = plan->paths;


  /* The rest of this section builds the critical path component for the
   * upgrade.  The way it works is quite simple.  Get the max bandwidth of each
   * pod and the core switches.  Get the number of switches that we are
   * upgrading in each pod/core.  The critical path would be the path with the
   * max bandwidth/#upgrades */

  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    uint32_t id = stg->pod_stats[i].pod_id;
    paths[id].bandwidth = stg->pod_stats[i].in.max;

    // Max number of switches to upgrade
    paths[id].sws = malloc(sizeof(struct jupiter_located_switch_t *) * expr->num_aggs_per_pod);
  }
  paths[num_groups - 1].bandwidth = stg->core_stats->in.max;
  paths[num_groups - 1].sws = malloc(sizeof(struct jupiter_located_switch_t *) * expr->num_cores);

  for (uint32_t i = 0; i < expr->nlocated_switches; ++i) {
    struct jupiter_located_switch_t *sw = &expr->located_switches[i];
    if (sw->type == JST_AGG) {
      paths[sw->pod].sws[paths[sw->pod].num_switches++] = sw;
    } else if (sw->type == JST_CORE) {
      paths[num_groups - 1].sws[paths[num_groups - 1].num_switches++] = sw;
    } else {
      panic("Unsupported switch type: %d", sw->type);
    }
  }

  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    uint32_t id = stg->pod_stats[i].pod_id;
    paths[id].bandwidth = stg->pod_stats[i].in.max;
  }
  paths[num_groups - 1].bandwidth = stg->core_stats->in.max;
  plan->num_paths = num_groups;
}

static risk_cost_t _exec_stg_best_subplan_at(
    struct exec_t *exec,
    struct expr_t *expr,
    trace_time_t at) {
  // STG doesn't really care about the best_plan_cost
  TO_STG(exec);

  struct stg_upgrade_plan_t *plan = stg->plan;
  qsort(plan->paths, plan->num_paths, sizeof(struct stg_critical_path_t), _cp_cmp);


  struct jupiter_located_switch_t **sws = malloc(
      sizeof(struct jupiter_located_switch_t *) * expr->nlocated_switches);


  unsigned nsteps = expr->criteria_time->steps;
  struct mop_t **mops = malloc(
      sizeof(struct mop_t *) * nsteps);

  // TODO: This should somehow use expr->criteria_time->acceptable
  for (uint32_t i = 0; i < nsteps; ++i) {
    unsigned idx = 0;
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

static struct exec_output_t *
_exec_stg_runner(struct exec_t *exec, struct expr_t *expr) {
  for (uint32_t i = expr->scenario.time_begin; i < expr->scenario.time_end; i += expr->scenario.time_step) {
    trace_time_t at = i;
    risk_cost_t actual_cost = _exec_stg_best_subplan_at(exec, expr, at);
    info("[%4d] Actual cost of the best plan (STG) is: %4.3f", at, actual_cost);
  }
  return 0;
}

struct exec_t *exec_stg_create(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_stg_t));

  exec->net_dp = 0;
  exec->validate = _exec_stg_validate;
  exec->run = _exec_stg_runner;

  return exec;
}
