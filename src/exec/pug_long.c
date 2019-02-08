#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

#include "algo/maxmin.h"
#include "util/common.h"
#include "config.h"
#include "dataplane.h"
#include "network.h"
#include "plan.h"
#include "predictor.h"
#include "risk.h"
#include "util/common.h"

#include "exec/pug_long.h"

#define TO_PUG(e) struct exec_pug_long_t *pug_long = (struct exec_pug_long_t *)e;
#define DEBUG(txt, ...) {}

// TODO: Criteria are in effect here ... Can add new criteria here or
// .. change later.  Too messy at the moment.
//
// - Omid 1/25/2019
static inline int
_best_plan_criteria(
    struct expr_t *expr,
    risk_cost_t p1_cost, int p1_length, double p1_perf,
    risk_cost_t p2_cost, int p2_length, double p2_perf) {

  return ( (p1_cost  < p2_cost) ||  // If cost was lower

          ((p1_cost == p2_cost) &&  // Or the length criteria was trumping
           (expr->criteria_plan_length(p1_length, p2_length) >  1)) ||

          ((p1_cost == p2_cost) &&  // Or the length criteria was trumping
           (expr->criteria_plan_length(p1_length, p2_length) == 0) &&
           (p1_perf > p2_perf)));
           //(p1_delta < p2_delta)));
}


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
_plan_invalidate_not_equal(struct plan_long_repo_t *repo, int subplan, int step) {
  uint32_t index = 0;
  uint32_t last_index = repo->plan_count - 1;

  int *ptr = repo->plans;
  int *last_ptr = repo->plans + repo->max_plan_size * last_index;
  int *tmp = malloc(sizeof(uint32_t) * repo->max_plan_size);

  int removed = 0;
  for (uint32_t i = 0; i < repo->plan_count; ++i ){
    int do_continue = 0;

    // Try to find the subplan in the remaining (step:max_plan_size) of this plan
    for (uint32_t j = step; j < repo->max_plan_size; ++j) {
      if (ptr[j] == subplan) {
        /* Put the subplan forward in the plan */
        ptr[j] = ptr[step];
        ptr[step] = subplan;

        /* Move the plan forward */
        ptr += repo->max_plan_size;
        index += 1;
        do_continue = 1;
        break;
      }
    }

    if (do_continue) {
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

  //DEBUG("removed: %d plans", removed);
  repo->plan_count = last_index + 1;
}

/* Returns the list of remaining subplans at step pug_long->plans->_cur_index. This
 * assumes that we have "fixed" the first pug_long->plans->_cur_index subplans,
 * i.e., we have taken those subplans */
static int *
_plans_remaining_subplans(struct exec_t *exec) {
  TO_PUG(exec);
  int idx = pug_long->plans->_cur_index;
  size_t size = sizeof(int) * pug_long->plans->_subplan_count;
  int *ret = malloc(size);
  memset(ret, 0, size);

  int *ptr = pug_long->plans->plans;
  int plan_size = pug_long->plans->max_plan_size;
  int plan_count = pug_long->plans->plan_count;

  for (uint32_t i = 0; i < plan_count; ++i) {
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
static struct plan_long_repo_t * __attribute__((unused)) 
_plans_get(struct exec_t *exec, struct expr_t const *expr) {
  TO_PUG(exec);
  uint32_t cap = 1000;

  struct jupiter_switch_plan_enumerator_t *en = 
    (struct jupiter_switch_plan_enumerator_t *)pug_long->planner;
  struct plan_iterator_t *iter = pug_long->iter;

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

  struct plan_long_repo_t *out = malloc(sizeof(struct plan_long_repo_t));
  out->plan_count = plan_count;
  out->initial_plan_count = plan_count;
  out->plans = plans;
  out->max_plan_size = max_plan_size;
  out->plan_size_in_bytes = plan_size_in_bytes;
  out->cap = cap;
  out->_subplan_count = iter->subplan_count(iter);
  out->_cur_index = 0;

  return out;
}

#define BUCKET_SIZE 1

static struct rvar_t *
_short_term_risk(struct exec_t *exec, struct expr_t *expr, int subplan, trace_time_t now) {
  TO_PUG(exec);
  struct rvar_sample_t *cost_rvar_tmp = (struct rvar_sample_t *)pug_long->steady_cost[subplan];
  return rvar_sample_create_with_vals(cost_rvar_tmp->vals, cost_rvar_tmp->num_samples);
}

static risk_cost_t
_long_term_best_plan_to_finish(struct exec_t *exec, struct expr_t *expr, 
    struct rvar_t *rvar, int idx, int *ret_plan_idx, int *ret_plan_length) {
  TO_PUG(exec);
  struct plan_long_repo_t *plans = pug_long->plans;
  int *ptr = plans->plans;

  risk_cost_t best_cost = INFINITY;
  int best_plan_idx = - 1;
  int best_plan_len = -1;
  struct risk_cost_func_t *viol_cost = expr->risk_violation_cost;
  struct rvar_t *best_risk = 0;

  struct rvar_t *cost_rvar = 0, *cost_rvar_tmp = 0;

  rvar_type_t short_term_cost = viol_cost->rvar_to_cost(viol_cost, rvar); //viol_cost->rvar_to_rvar(viol_cost, rvar, 0);

  rvar_type_t vals[] = {0};
  struct rvar_t *zero_rvar = rvar_sample_create_with_vals(vals, 1);

  for (uint32_t i = 0; i < plans->plan_count; ++i) {
    int plan_len = 0;
    cost_rvar = (struct rvar_t *)zero_rvar->to_bucket(zero_rvar, BUCKET_SIZE);

    // Build the cost of the remainder of the plan, aka, long-term
    for (uint32_t j = idx; j < plans->max_plan_size; ++j) {
      // If there are no subplans left just return.
      if (ptr[j] == 0)
        break;

      for (uint32_t dur = 0; dur < expr->mop_duration; ++dur) {
        cost_rvar_tmp = pug_long->steady_cost[ptr[j]];
        cost_rvar_tmp = cost_rvar_tmp->convolve(cost_rvar_tmp, cost_rvar, BUCKET_SIZE);
        cost_rvar->free(cost_rvar);
        cost_rvar = cost_rvar_tmp;
      }

      plan_len++;
    }
    
    // Move to the next plan
    ptr += plans->max_plan_size;

    // Calculate the cost of the remainder of the plan and sum it up with the short-term cost
    risk_cost_t cost = (viol_cost->rvar_to_cost(viol_cost, cost_rvar) / (double)(plan_len + 1));
    cost += short_term_cost;

    if  (_best_plan_criteria(expr, cost, plan_len, 10, 
                                   best_cost, best_plan_len, 10)) {
      best_plan_idx = i;
      best_cost = cost;
      best_plan_len = plan_len;
      if (best_risk)
        best_risk->free(best_risk);
      best_risk = cost_rvar;
    } else {
      cost_rvar->free(cost_rvar);
    }
  }

  if (expr->verbose >= VERBOSE_SHOW_ME_PLAN_RISK) {
    if (best_risk)
      best_risk->plot(best_risk);
  }

  if (best_risk)
    best_risk->free(best_risk);
  *ret_plan_idx = best_plan_idx;
  *ret_plan_length = best_plan_len;
  return best_cost;
}

static int
_exec_pug_long_find_best_next_subplan(struct exec_t *exec,
    struct expr_t *expr, trace_time_t at, risk_cost_t *ret_cost,
    int *ret_plan_len, int *ret_plan) {
  TO_PUG(exec);
  struct plan_long_repo_t *plans = pug_long->plans;
  risk_cost_t  best_plan_cost = INFINITY;
  int          best_plan_len = -1;
  double       best_pref_score = 0;

  int         *best_plan_subplans  = ret_plan;

  int *subplans = _plans_remaining_subplans(exec);
  int finished = 1;
  int best_subplan = 0;
  for (uint32_t i = 1; i < pug_long->plans->_subplan_count; ++i) {
    if (subplans[i] != 1)
      continue;

    finished = 0;
    /* TODO: This is very hacky atm, but unfortunately, there is not enough
     * time to do it properly. So the idea of this is:
     *
     * 1) Choose a subplan.
     * 2) Fix that subplan, i.e., prune the search space to plans that have
     * that subplan.
     * 3) Find the best "long-term" sequence of actions to finish that plan
     * 4) Unfix that subplan and move to the next subplan.
     *
     * By fixing and unfixing I mean playing with the plans->_cur_index and
     * plans->plan_count values.
     *
     * - Omid 1/25/2019
     * */

    /* Fix the plans */
    int plan_count = plans->plan_count;
    int plan_idx = 0;
    int plan_length = 0;
    double pref_score = pug_long->iter->pref_score(pug_long->iter, i);
    _plan_invalidate_not_equal(plans, i, plans->_cur_index);

    // Assess the short term risk for subplans[i]
    // info("Subplan %d", i);
    struct rvar_t *st_risk = _short_term_risk(exec, expr, i, at);
    risk_cost_t cost = _long_term_best_plan_to_finish(exec, expr, 
        st_risk, plans->_cur_index + 1, &plan_idx, &plan_length);
    // info("Total cost to finish: %lf", cost);

    if (_best_plan_criteria(expr,
          cost, plan_length, pref_score,
          best_plan_cost, best_plan_len, best_pref_score)) {
      best_plan_cost = cost;
      best_plan_len = plan_length;
      best_pref_score = pref_score;
      best_subplan = i;
      memcpy( best_plan_subplans, 
          plans->plans + (plan_idx * plans->max_plan_size), 
          plans->plan_size_in_bytes);
    }


    // Recover the plans that we stopped looking into
    plans->plan_count = plan_count;

    // info("Expected risk of running subplan %d is %f", i, st_risk->expected(st_risk));
    // Get the best long-term plan to finish subplans[i]
  }

  if (!finished) {
    *ret_cost = best_plan_cost;
    *ret_plan_len = best_plan_len;
  }

  DEBUG(">>>> Choosing subplan %d with cost %lf", best_subplan, best_plan_cost);

  return finished;
}

static risk_cost_t
_exec_pug_long_best_plan_at(struct exec_t *exec, struct expr_t *expr, trace_time_t at,
    risk_cost_t *best_plan_cost, int *best_plan_len, int *best_plan_subplans) {
  TO_PUG(exec);

  int finished = 0;
  risk_cost_t running_cost = 0;
  struct plan_long_repo_t *plans = pug_long->plans;

  plans->_cur_index = 0;
  plans->plan_count = plans->initial_plan_count;

  while (1) {
    finished = _exec_pug_long_find_best_next_subplan(
        exec, expr, at, best_plan_cost, best_plan_len, best_plan_subplans);

    // pug_long->iter->explain(pug_long->iter, best_plan_subplans[plans->_cur_index]);

    if (finished)
      break;

    DEBUG("Estimated cost of %d(th) subplan is: %f", plans->_cur_index, *best_plan_cost);
    _plan_invalidate_not_equal(
        plans, best_plan_subplans[plans->_cur_index], plans->_cur_index);
    plans->_cur_index += 1;

    running_cost += *best_plan_cost;
    at += expr->mop_duration;
  }
  *best_plan_len = plans->_cur_index;
  return running_cost;
}

static struct mop_t **
_exec_mops_for_create(struct exec_t *exec, struct expr_t *expr,
    int *subplans, int nsubplan) {
  TO_PUG(exec);
  struct mop_t **mops = malloc(sizeof(struct mop_t *) * nsubplan);
  for (uint32_t i = 0; i < nsubplan; ++i) {
    mops[i] = pug_long->iter->mop_for(pug_long->iter, subplans[i]);
  }

  return mops;
}

static void
_exec_mops_for_free(struct exec_t *exec, struct expr_t *expr,
    struct mop_t **mops, int nsubplan) {
  for (uint32_t i = 0; i < nsubplan; ++i) {
    mops[i]->free(mops[i]);
  }
  free(mops);
}

static void
_exec_pug_long_validate(struct exec_t *exec, struct expr_t const *expr) {
  int subplan_count = 0;
  TO_PUG(exec);

  pug_long->trace = traffic_matrix_trace_load(400, expr->traffic_test);
  if (pug_long->trace == 0)
    panic("Couldn't load the traffic matrix file: %s", expr->traffic_test);

  pug_long->trace_training = traffic_matrix_trace_load(400, expr->traffic_training);
  if (pug_long->trace == 0)
    panic("Couldn't load the training traffic matrix file: %s", expr->traffic_test);

  pug_long->pred = exec_predictor_create(exec, expr, expr->predictor_string);

  pug_long->steady_packet_loss = exec_rvar_cache_load(expr, &subplan_count);
  if (pug_long->steady_packet_loss == 0)
    panic("Couldn't load the long-term RVAR cache.");

  /* Create the cost variables */
  if (expr->risk_violation_cost == 0)
    panic("Risk of violation cost not set.");

  pug_long->steady_cost = malloc(sizeof(struct rvar_t *) * subplan_count);
  info("Creating the steady cost random variables: %d", subplan_count);
  for (uint32_t i = 0; i < subplan_count; ++i) {
    pug_long->steady_cost[i] = expr->risk_violation_cost->rvar_to_rvar(
        expr->risk_violation_cost, pug_long->steady_packet_loss[i], 0);
    // info("Plot of packet loss for subplan %d", i);
    // pug_long->steady_packet_loss[i]->plot(pug_long->steady_packet_loss[i]);
    // info("Plot of cost for subplan %d", i);
    // pug_long->steady_cost[i]->plot(pug_long->steady_cost[i]);
  }

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
  pug_long->iter = en->iter((struct plan_t *)en);
  pug_long->planner = (struct plan_t *)en;

  pug_long->plans = _plans_get(exec, expr);
  if (pug_long->plans == 0)
    panic("Couldn't build the plan repository.");

  info("Found %d valid plans.", pug_long->plans->plan_count);
}

static void 
_exec_pug_long_runner(struct exec_t *exec, struct expr_t *expr) {
  TO_PUG(exec);

  struct plan_long_repo_t *plans = pug_long->plans;
  risk_cost_t  best_plan_cost = INFINITY;
  int          best_plan_len = -1;
  int         *best_plan_subplans  = malloc(sizeof(int) * plans->max_plan_size);

  for (uint32_t i = expr->scenario.time_begin; i < expr->scenario.time_end; i += expr->scenario.time_step) {
    trace_time_t at = i;

    risk_cost_t estimated_cost = _exec_pug_long_best_plan_at(
        exec, expr, at, &best_plan_cost, &best_plan_len, best_plan_subplans);
    struct mop_t **mops = _exec_mops_for_create(
        exec, expr, best_plan_subplans, best_plan_len);
    risk_cost_t actual_cost = exec_plan_cost(
        exec, expr, mops, best_plan_len, at);
    _exec_mops_for_free(exec, expr, mops, best_plan_len);

    info("[%4d] Actual cost of the best plan (%d) is: %4.3f : %4.3f",
        at, best_plan_len, actual_cost, estimated_cost);
  }
  free(best_plan_subplans);
}

static void
_exec_pug_long_explain(struct exec_t *exec) {
  text_block(
			 "Pug long uses long-term traffic estimates to find plans.");
}

struct exec_t *exec_pug_long_create(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_pug_long_t));
  exec->net_dp = 0;

  exec->validate = _exec_pug_long_validate;
  exec->run = _exec_pug_long_runner;
  exec->explain = _exec_pug_long_explain;

  return exec;
}
