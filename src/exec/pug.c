#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

#include "algo/maxmin.h"
#include "util/common.h"
#include "config.h"
#include "dataplane.h"
#include "network.h"
#include "freelist.h"
#include "plan.h"
#include "predictor.h"
#include "predictors/ewma.h"
#include "risk.h"
#include "util/common.h"

#include "exec/pug.h"

#define TO_PUG(e) struct exec_pug_t *pug = (struct exec_pug_t *)e;

static bw_t __attribute__((unused))
_tm_sum(struct traffic_matrix_t *tm) {
  bw_t tot_bw = 0;
  for (uint32_t j = 0; j < tm->num_pairs; ++j) {
    tot_bw += tm->bws[j].bw;
  }

  return tot_bw;
}

/* Invalidates/removes plans that don't have "subplan" in "step"'s subplan (or after)
 * This prunes the subplan search space to subplans */
static void __attribute__((unused))
_plan_invalidate_not_equal(struct plan_repo_t *repo, int subplan, int step) {
  uint32_t index = 0;
  uint32_t last_index = repo->plan_count - 1;

  int *ptr = repo->plans;
  int *last_ptr = repo->plans + repo->max_plan_size * last_index;
  int *tmp = malloc(sizeof(uint32_t) * repo->max_plan_size);

  int removed = 0;
  for (uint32_t i = 0; i < repo->plan_count; ++i ){
    int do_continue = 0;
    for (uint32_t j = step; j < repo->max_plan_size; ++j) {
      if (ptr[j] == subplan) {
        /* Put the subplan forward */
        ptr[j] = ptr[step];
        ptr[step] = subplan;

        /* Move the plan forward */
        ptr += repo->max_plan_size;
        index += 1;
        do_continue = 1;
        break;
      }
    }

    if (do_continue)
      continue;

    if (ptr[step] == subplan) {
      ptr += repo->max_plan_size;
      index += 1;
      continue;
    }

    // Remove plan by moving it to the end and reducing the num plans
    memcpy(tmp, last_ptr, repo->plan_size_in_bytes);
    memcpy(last_ptr, ptr, repo->plan_size_in_bytes);
    memcpy(ptr, tmp, repo->plan_size_in_bytes);

    last_index -= 1;
    last_ptr -= repo->max_plan_size;
    removed += 1;
  }

  info("removed: %d plans", removed);
  repo->plan_count = last_index + 1;
}

/* Returns the list of valid subplans at step pug->plans->_cur_index. This
 * assumes that we have "fixed" the first pug->plans->_cur_index subplans,
 * i.e., we have taken those subplans */
static int *
_plans_valid_subplans(struct exec_t *exec) {
  TO_PUG(exec);
  int idx = pug->plans->_cur_index;
  int *ret = malloc(sizeof(uint32_t) * pug->plans->_subplan_count);

  int *ptr = pug->plans->plans;
  int plan_size = pug->plans->max_plan_size;

  for (uint32_t i = 0; i < pug->plans->plan_count; ++i) {
    for (uint32_t j = idx; j < plan_size; ++j) {
      ret[ptr[j]] = 1;
    }
    ptr += plan_size;
  }

  return ret;
}

/* Returns the list of plans matching the exec->criteria_time requirements
 * TODO: Make the criteria more streamlined.  Right now it's hidden here and we
 * only deal with time criteria.
 *
 * Omid - 1/25/2019
 * */
static struct plan_repo_t * __attribute__((unused)) 
_plans_get(struct exec_t *exec, struct expr_t const *expr) {
  TO_PUG(exec);
  uint32_t cap = 1000;

  struct jupiter_switch_plan_enumerator_t *en = 
    (struct jupiter_switch_plan_enumerator_t *)pug->planner;
  struct plan_iterator_t *iter = pug->iter;

  // Number of plans collected so far.
  int plan_count = 0;
  // Maximum length of a plan.
  int max_plan_size = 0;
  for (uint32_t i = 0; i < en->multigroup.ngroups; ++i) {
    max_plan_size += en->multigroup.groups[i].group_size;
  }

  int plan_size_in_bytes = sizeof(int) * max_plan_size;
  int *plans = malloc(cap * plan_size_in_bytes);
  int *subplans = 0; int subplan_count;
  int *plan_ptr = plans;

  for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
    iter->plan(iter, &subplans, &subplan_count);
    
    if (!expr->criteria_time->acceptable(expr->criteria_time, subplan_count))
      continue;

    memset(plan_ptr, 0, plan_size_in_bytes);
    memcpy(plan_ptr, subplans, sizeof(int) * subplan_count);
    plan_ptr += max_plan_size;

    plan_count++;

    if (plan_count >= cap) {
      cap *= 2;
      plans = realloc(plans, cap * plan_size_in_bytes);
      plan_ptr = plans + max_plan_size * plan_count;
    }

    free(subplans);
  }

  struct plan_repo_t *out = malloc(sizeof(struct plan_repo_t));
  out->plan_count = plan_count;
  out->plans = plans;
  out->max_plan_size = max_plan_size;
  out->plan_size_in_bytes = plan_size_in_bytes;
  out->cap = cap;
  out->_subplan_count = iter->subplan_count(iter);
  out->_cur_index = 0;

  return out;
}

static void
_exec_pug_validate(struct exec_t *exec, struct expr_t const *expr) {
  int subplan_count = 0;
  TO_PUG(exec);

  pug->trace = traffic_matrix_trace_load(400, expr->traffic_test);
  if (pug->trace == 0)
    panic("Couldn't load the traffic matrix file: %s", expr->traffic_test);

  pug->pred = exec_ewma_cache_build_or_load(expr);
  if (pug->pred == 0)
    panic("Could't load the predictor.");

  pug->steady_packet_loss = exec_rvar_cache_load(expr, &subplan_count);
  if (pug->steady_packet_loss == 0)
    panic("Couldn't load the long-term RVAR cache.");

  if (expr->criteria_time == 0)
    panic("Time criteria not set.");

  /* TODO: This shouldn't be jupiter specific 
   *
   * Omid - 1/25/2019
   * */
  struct jupiter_switch_plan_enumerator_t *en = jupiter_switch_plan_enumerator_create(
      expr->upgrade_list.num_switches,
      expr->located_switches,
      expr->upgrade_freedom,
      expr->upgrade_nfreedom);
  pug->iter = en->iter((struct plan_t *)en);
  pug->planner = (struct plan_t *)en;

  pug->plans = _plans_get(exec, expr);
  if (pug->plans == 0)
    panic("Couldn't build the plan repository.");

  info("Found %d valid plans.", pug->plans->plan_count);
}

static struct rvar_t *
_short_term_risk(struct exec_t *exec, struct expr_t *expr,
    int subplan, trace_time_t now, trace_time_t low, trace_time_t high) {
  TO_PUG(exec);
  struct mop_t *mop = pug->iter->mop_for(pug->iter, subplan);
  int tm_count = (high - low + 1) * expr->mop_duration;
  struct traffic_matrix_t **tms = malloc(
      sizeof(struct traffic_matrix_t *) * tm_count);
  struct predictor_t *pred = pug->pred;
  struct traffic_matrix_trace_t *trace = pug->trace;

  int index = 0;
  struct traffic_matrix_t *tm = 0;
  traffic_matrix_trace_get(trace, now, &tm);

  for (uint32_t i = low; i <= high; ++i) {
    struct predictor_iterator_t *iter = pred->predict(pred, tm, i, i + expr->mop_duration);
    for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
      tms[index++] = iter->cur(iter);
    }
  }

  assert(index == tm_count);
  struct rvar_t *ret = exec_simulate(exec, expr, mop, tms, tm_count);

  /* Free the allocated traffic matrices */
  for (uint32_t i = 0; i < tm_count; ++i) {
    traffic_matrix_free(tms[i]);
  }
  traffic_matrix_free(tm);

  return ret;
}

static void
_exec_pug_prepare_env(struct exec_t *exec) {
}

static void 
_exec_pug_runner(struct exec_t *exec, struct expr_t *expr) {
  trace_time_t at = 50;
  //risk_cost_t cost = 0;
  TO_PUG(exec);

  _exec_pug_prepare_env(exec);
  struct traffic_matrix_trace_iter_t *iter = pug->trace->iter(pug->trace);
  iter->go_to(iter, at);

  info("Running pug runner.");

  //while (1) {
    trace_time_t err_low  = at - 10;
    trace_time_t err_high = at + 10;

    int *subplans = _plans_valid_subplans(exec);
    for (uint32_t i = 0; i < pug->plans->_subplan_count; ++i) {
      if (subplans[i] == 0)
        continue;

      // Assess the short term risk for subplans[i]
      struct rvar_t *st_risk = _short_term_risk(exec, expr, i, at, err_low, err_high);
      info("Expected risk of running subplan %d is %f", i, st_risk->expected(st_risk));
      // Get the best long-term plan to finish subplans[i]
    }
  //}
}

struct exec_t *exec_pug_create(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_pug_t));

  exec->validate = _exec_pug_validate;
  exec->run = _exec_pug_runner;

  return exec;
}
