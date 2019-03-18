#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>

#include "algo/array.h"
#include "algo/maxmin.h"
#include "util/common.h"
#include "util/debug.h"
#include "config.h"
#include "dataplane.h"
#include "failure.h"
#include "network.h"
#include "plan.h"
#include "predictor.h"
#include "risk.h"

#include "exec/pug.h"

#define TO_PUG(e) struct exec_pug_t *pug = (struct exec_pug_t *)e;
#define EXP(p) ((p)->expected((struct rvar_t *)(p)))

#if DEBUG_MODE == 1
#define DEBUG(txt, ...) info(txt, ##__VA_ARGS__);
#else 
#define DEBUG(txt, ...) {}
#endif

#define BUCKET_SIZE 1

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
_plan_invalidate_not_equal(struct plan_repo_t *repo, int subplan, int step) {
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
  free(tmp);
}

/* Returns the list of remaining subplans at step pug->plans->_cur_index. This
 * assumes that we have "fixed" the first pug->plans->_cur_index subplans,
 * i.e., we have taken those subplans */
static int *
_plans_remaining_subplans(struct exec_t *exec) {
  TO_PUG(exec);
  int idx = pug->plans->_cur_index;
  size_t size = sizeof(int) * pug->plans->_subplan_count;
  int *ret = malloc(size);
  memset(ret, 0, size);

  int *ptr = pug->plans->plans;
  int plan_size = pug->plans->max_plan_size;
  int plan_count = pug->plans->plan_count;

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
    
    if (!expr->criteria_time->acceptable(expr->criteria_time, subplan_count)) {
      free(subplans);
      continue;
    }

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
  out->initial_plan_count = plan_count;
  out->plans = plans;
  out->max_plan_size = max_plan_size;
  out->plan_size_in_bytes = plan_size_in_bytes;
  out->cap = cap;
  out->_subplan_count = iter->subplan_count(iter);
  out->_cur_index = 0;

  return out;
}

static struct rvar_t *
_short_term_risk_using_long_term_cache(struct exec_t *exec, struct expr_t *expr, int subplan, trace_time_t now) {
  TO_PUG(exec);
  struct rvar_t *rv = pug->steady_cost[subplan];
  struct rvar_t *ret = rv->copy(rv);
  return ret;
}

static struct rvar_t *
_short_term_risk_using_predictor(struct exec_t *exec, struct expr_t *expr,
    int subplan, trace_time_t now) {
  TO_PUG(exec);
  struct mop_t *mop = pug->iter->mop_for(pug->iter, subplan);

  struct predictor_t *pred = pug->pred;
  struct predictor_iterator_t *iter = pred->predict(pred, now, now + expr->mop_duration);

  int num_samples = iter->length(iter);
  int tm_count = num_samples * expr->mop_duration;
  struct traffic_matrix_t **tms = malloc(
      sizeof(struct traffic_matrix_t *) * tm_count);
  int index = 0;

  for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
    struct traffic_matrix_trace_iter_t *titer = iter->cur(iter);
    for (titer->begin(titer); !titer->end(titer); titer->next(titer)) {
      struct traffic_matrix_t *tm = 0;
      titer->get(titer, &tm);
      tms[index++] = tm;
    }
    titer->free(titer);
  }
  iter->free(iter);
  assert(tm_count == index);

  /* The returned rvar_types are in the order they were passed to exec_simulate_ordered */
  rvar_type_t *vals = exec_simulate_ordered(exec, expr, mop, tms, tm_count);

  /* Free the allocated traffic matrices */
  for (uint32_t i = 0; i < tm_count; ++i) {
    traffic_matrix_free(tms[i]);
  }

  /* Build the rvar for the cost of the short term planner */
  index = 0;
  struct risk_cost_func_t *func = expr->risk_violation_cost;
  rvar_type_t *costs = malloc(sizeof(rvar_type_t) * num_samples);
  for (uint32_t i = 0; i < num_samples; ++i) {
    costs[i] = 0;
    for (uint32_t j = 0; j < expr->mop_duration; ++j) {
      costs[i] += func->cost(func, vals[index]);
      index++;
    }
  }

  struct rvar_t *ret = rvar_sample_create_with_vals(costs, num_samples);
  mop->free(mop);
  return ret;
}


static risk_cost_t
_term_best_plan_to_finish(struct exec_t *exec, struct expr_t *expr, 
    struct rvar_t *rvar, int idx, int *ret_plan_idx, int *ret_plan_length,
    int cur_step) {
  TO_PUG(exec);
  struct plan_repo_t *plans = pug->plans;
  int *ptr = plans->plans;

  risk_cost_t best_cost = INFINITY;
  int best_plan_idx = - 1;
  int best_plan_len = -1;
  struct risk_cost_func_t *viol_cost = expr->risk_violation_cost;
  struct rvar_t *best_risk = 0;

  struct rvar_t *cost_rvar = 0, *cost_rvar_tmp = 0;

  // Create a zeroed rvar for initial cost
  struct rvar_t *zero_rvar = rvar_zero();

  for (uint32_t i = 0; i < plans->plan_count; ++i) {
    int plan_len = 0;
    cost_rvar = (struct rvar_t *)zero_rvar->to_bucket(zero_rvar, BUCKET_SIZE);

    // Build the cost of the remainder of the plan, aka, long-term
    for (uint32_t j = idx; j < plans->max_plan_size; ++j) {
      // If there are no subplans left just return.
      if (ptr[j] == 0)
        break;

      for (uint32_t dur = 0; dur < expr->mop_duration; ++dur) {
        cost_rvar_tmp = pug->steady_cost[ptr[j]];
        cost_rvar_tmp = cost_rvar_tmp->convolve(cost_rvar_tmp, cost_rvar, BUCKET_SIZE);
        cost_rvar->free(cost_rvar);
        cost_rvar = cost_rvar_tmp;
      }

      plan_len++;
    }
    
    // Move to the next plan
    ptr += plans->max_plan_size;

    struct rvar_t *sum = cost_rvar->convolve(cost_rvar, rvar, BUCKET_SIZE);
    risk_cost_t cost = viol_cost->rvar_to_cost(viol_cost, sum);

    // TODO: Do I need to change the cost anywhere else?
    cost += expr->criteria_time->cost(expr->criteria_time, cur_step + plan_len + 1); 

    sum->free(sum);

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

  zero_rvar->free(zero_rvar);
  //DEBUG("BEST LONG TERM COST: %f", best_term_cost);
  //DEBUG("BEST SHORT TERM COST: %f", best_short_term_cost);
  return best_cost;
}

static int
_exec_pug_find_best_next_subplan(struct exec_t *exec,
    struct expr_t *expr, trace_time_t at, risk_cost_t *ret_cost,
    int *ret_plan_len, int *ret_plan, int cur_step) {
  TO_PUG(exec);
  struct plan_repo_t *plans = pug->plans;
  risk_cost_t  best_plan_cost = INFINITY;
  int          best_plan_len = -1;
  double       best_pref_score = 0;
  int          best_subplan = 0;
  int         *best_plan_subplans  = ret_plan;

  int *subplans = _plans_remaining_subplans(exec);
  int finished = 1;
  for (uint32_t i = 1; i < pug->plans->_subplan_count; ++i) {
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
    double pref_score = pug->iter->pref_score(pug->iter, i);
    _plan_invalidate_not_equal(plans, i, plans->_cur_index);

#if DEBUG_MODE
    /* TODO: Can remove */
    struct mop_t *mop = pug->iter->mop_for(pug->iter, i);
    char *ret = mop->explain(mop, expr->network);
    DEBUG("Short Term Subplan %s", ret);
    mop->free(mop);
    free(ret);
#endif

    // Assess the short term risk for subplans[i]
    struct rvar_t *st_risk = pug->short_term_risk(exec, expr, i, at);
    risk_cost_t cost = _term_best_plan_to_finish(exec, expr, 
        st_risk, plans->_cur_index + 1, &plan_idx, &plan_length, cur_step);

#if DEBUG_MODE
    DEBUG("Short Term Cost: %lf", expr->risk_violation_cost->rvar_to_cost(expr->risk_violation_cost, st_risk));
    info("Total cost to finish: %lf", cost);
#endif
    st_risk->free(st_risk);

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

  (void)(best_subplan);
  DEBUG(">>>> Choosing subplan %d with cost %lf", best_subplan, best_plan_cost);

  return finished;
}

static void __attribute__((unused))
_print_freedom_plan(struct exec_t *exec, struct expr_t *expr, 
    int best_subplan_len, int *best_plan_subplans) {
  TO_PUG(exec);
  struct plan_repo_t *plans = pug->plans;

  char *fin = malloc(2048 * sizeof(char));
  memset(fin, 0, 2048);
  for (uint32_t i = plans->_cur_index; i < plans->max_plan_size; ++i)  {
    if (best_plan_subplans[i] == 0)
      break;
    char *ret = pug->iter->explain(pug->iter, best_plan_subplans[i]);
    strcat(fin, ret);
    strcat(fin, ", ");
    free(ret);
  }
  info("PLAN: %s", fin);
  free(fin);
}

static risk_cost_t
_exec_pug_best_plan_at(struct exec_t *exec, struct expr_t *expr, trace_time_t at,
    risk_cost_t *best_plan_cost, int *best_plan_len, int *best_plan_subplans) {
  TO_PUG(exec);

  int finished = 0;
  risk_cost_t running_cost = 0;
  struct plan_repo_t *plans = pug->plans;

  plans->_cur_index = 0;
  plans->plan_count = plans->initial_plan_count;

  while (1) {
    finished = _exec_pug_find_best_next_subplan(
        exec, expr, at, best_plan_cost, best_plan_len, best_plan_subplans,
        plans->_cur_index);

#if DEBUG_MODE
    _print_freedom_plan(exec, expr, *best_plan_len, best_plan_subplans);
#endif

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
    mops[i] = pug->iter->mop_for(pug->iter, subplans[i]);
  }

#if DEBUG_MODE
  for (uint32_t i = 0; i < nsubplan; ++i) {
    char *desc = mops[i]->explain(mops[i], expr->network);
    info("MOp explanation: %s", desc);
    free(desc);
  }
#endif

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
_exec_pug_validate(struct exec_t *exec, struct expr_t const *expr) {
  TO_PUG(exec);

  pug->trace = traffic_matrix_trace_load(400, expr->traffic_test);
  if (pug->trace == 0)
    panic("Couldn't load the traffic matrix file: %s", expr->traffic_test);

  pug->trace_training = traffic_matrix_trace_load(400, expr->traffic_training);
  if (pug->trace == 0)
    panic("Couldn't load the training traffic matrix file: %s", expr->traffic_test);

  pug->pred = exec_predictor_create(exec, expr, expr->predictor_string);

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

static struct exec_output_t *
_exec_pug_runner(struct exec_t *exec, struct expr_t *expr) {
  TO_PUG(exec);

  struct exec_output_t *res = malloc(sizeof(struct exec_output_t));
  struct exec_result_t result = {0};
  res->result = array_create(sizeof(struct exec_result_t), 10);

  struct plan_repo_t *plans = pug->plans;
  risk_cost_t  best_plan_cost = INFINITY;
  int          best_plan_len = -1;
  int         *best_plan_subplans  = malloc(sizeof(int) * plans->max_plan_size);

  for (uint32_t i = expr->scenario.time_begin; i < expr->scenario.time_end; i += expr->scenario.time_step) {
    trace_time_t at = i;

    pug->prepare_steady_cost(exec, expr, at);

    risk_cost_t estimated_cost = _exec_pug_best_plan_at(
        exec, expr, at, &best_plan_cost, &best_plan_len, best_plan_subplans);
    struct mop_t **mops = _exec_mops_for_create(
        exec, expr, best_plan_subplans, best_plan_len);
    risk_cost_t actual_cost = exec_plan_cost(
        exec, expr, mops, best_plan_len, at);
    _exec_mops_for_free(exec, expr, mops, best_plan_len);

    info("[%4d] Actual cost of the best plan (%02d) is: %4.3f : %4.3f",
        at, best_plan_len, actual_cost, estimated_cost);

    result.at = i;
    result.num_steps = best_plan_len;
    result.description = 0;
    result.cost = actual_cost;

    array_append(res->result, &result);
    pug->release_steady_cost(exec, expr, at);
  }
  free(best_plan_subplans);

  return res;
}

static void
_exec_pug_long_explain(struct exec_t *exec) {
  text_block(
			 "Pug long uses long-term traffic estimates to find plans.");
}

static void
_exec_pug_short_and_long_explain(struct exec_t *exec) {
  text_block(
      "Pug uses short-term + long-term traffic estimates to find plans.\n"
      "You can set the predictor that pug uses through the .ini file.");
}

static void
prepare_steady_cost_static(struct exec_t *exec, struct expr_t *expr, trace_time_t time) {
  TO_PUG(exec);
  // We have already loaded the steady_packet_loss
  if (pug->steady_packet_loss != 0)
    return;

  // Load the steady_packet_loss data
  int subplan_count = 0;
  pug->steady_packet_loss = exec_rvar_cache_load(expr, &subplan_count);
  if (pug->steady_packet_loss == 0)
    panic("Couldn't load the long-term RVAR cache.");

  /* Create the cost variables */
  if (expr->risk_violation_cost == 0)
    panic("Risk of violation cost not set.");

  struct rvar_t **rcache =  malloc(sizeof(struct rvar_t *) * subplan_count);
  info("Preparing the steady_cost cache.");
  for (uint32_t i = 0; i < subplan_count; ++i) {
    struct rvar_t *rv = expr->risk_violation_cost->rvar_to_rvar(
        expr->risk_violation_cost, pug->steady_packet_loss[i], 0);
    //rv->plot(rv);
    rcache[i] = (struct rvar_t *)rv->to_bucket(rv, BUCKET_SIZE);
    //pug->steady_cost[i]->plot(pug->steady_cost[i]);
    rv->free(rv);
  }

  // Create the actual steady cost values
  pug->steady_cost = malloc(sizeof(struct rvar_t *) * subplan_count);
  struct plan_iterator_t *iter = pug->planner->iter(pug->planner);
  for (uint32_t i = 0; i < subplan_count; ++i) {
    pug->steady_cost[i] = expr->failure->apply(expr->failure, expr->network, iter, rcache, i);

    /*
    struct mop_t *mop = pug->iter->mop_for(pug->iter, i);
    char *out = mop->explain(mop, expr->network);
    info("%s", out);
    free(out);
    free(mop);
    */

    info("Expected cost before failure considerations %d: %f, after: %f", 
        i, rcache[i]->expected(rcache[i]), 
        pug->steady_cost[i]->expected(pug->steady_cost[i]));
  }
  iter->free(iter);

  // Free the cost resources
  for (uint32_t i = 0; i < subplan_count; ++i) {
    rcache[i]->free(rcache[i]);
  }
  free(rcache);

  info("Done preparing the steady_cost cache.");
}

static void
release_steady_cost_static(struct exec_t *exec, struct expr_t *expr, trace_time_t time) {
  // Does nothing
  return;
}

static void
prepare_steady_cost_dynamic(struct exec_t *exec, struct expr_t *expr, trace_time_t time) {
  TO_PUG(exec);

  int subplan_count = 0;
  struct array_t **arr = exec_rvar_cache_load_into_array(expr, &subplan_count);
  if (arr == 0) {
    panic("Couldn't load the RVAR array.");
    return;
  }

  /* Create the cost variables */
  if (expr->risk_violation_cost == 0)
    panic("Risk of violation cost not set.");

  assert(pug->steady_packet_loss == 0);
  assert(pug->steady_cost == 0);

  pug->steady_packet_loss = malloc(sizeof(struct rvar_t *) * subplan_count);
  pug->steady_cost = malloc(sizeof(struct rvar_t *) * subplan_count);

  trace_time_t start, end;

  trace_time_t backtrack_time = expr->pug_backtrack_traffic_count;
  int pug_backtrack = expr->pug_is_backtrack;

  if (pug_backtrack) {
    end = time;
    if (time < backtrack_time) {
      start = 0;
    } else {
      start = time - backtrack_time;
    }
    if (start == end) {
      end = start + 1;
    }
  } else {
    start = time;
    end = start + backtrack_time;
    if (end >= exec->trace->num_indices) {
      end = exec->trace->num_indices - 1;
    }
    if (start >= end) {
      start = end - 1;
    }
  }

  struct rvar_t **rcache = malloc(sizeof(struct rvar_t *) * subplan_count);
  for (uint32_t i = 0; i < subplan_count; ++i) {
    void *data = 0; int data_size = 0;
    data = array_splice(arr[i], start, end, &data_size);
    pug->steady_packet_loss[i] = rvar_sample_create_with_vals(data, data_size);

    struct rvar_t *rv = expr->risk_violation_cost->rvar_to_rvar(
        expr->risk_violation_cost, pug->steady_packet_loss[i], 0);
    rcache[i] = (struct rvar_t *)rv->to_bucket(rv, BUCKET_SIZE);
    rv->free(rv);
  }

  /* Apply the failure model to long term plans */
  for (uint32_t i = 0; i < subplan_count; ++i) {
    pug->steady_cost[i] = expr->failure->apply(
        expr->failure, expr->network,
        pug->iter, rcache, i);
  }

  for (uint32_t i = 0; i < subplan_count; ++i) {
    rcache[i]->free(rcache[i]);
  }
  free(rcache);
}

static void
release_steady_cost_dynamic(struct exec_t *exec, struct expr_t *expr, trace_time_t time) {
  TO_PUG(exec);
  // Things are already released no need to do anything about them.
  if (!pug->steady_packet_loss)
    return;

  for (int i = 0; i < pug->plans->_subplan_count; ++i) {
    struct rvar_t *rv = pug->steady_packet_loss[i];
    rv->free(rv);

    rv = pug->steady_cost[i];
    rv->free(rv);
  }

  free(pug->steady_packet_loss);
  free(pug->steady_cost);

  pug->steady_packet_loss = 0;
  pug->steady_cost = 0;
}

struct exec_t *exec_pug_create_short_and_long_term(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_pug_t));
  exec->net_dp = 0;

  exec->validate = _exec_pug_validate;
  exec->run = _exec_pug_runner;
  exec->explain = _exec_pug_short_and_long_explain;

  TO_PUG(exec);
  pug->steady_packet_loss = 0;
  pug->steady_cost = 0;
  pug->short_term_risk = _short_term_risk_using_predictor;
  pug->prepare_steady_cost = prepare_steady_cost_static;
  pug->release_steady_cost = release_steady_cost_static;

  return exec;
}

struct exec_t *exec_pug_create_long_term_only(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_pug_t));
  exec->net_dp = 0;

  exec->validate = _exec_pug_validate;
  exec->run = _exec_pug_runner;
  exec->explain = _exec_pug_long_explain;

  TO_PUG(exec);
  pug->steady_packet_loss = 0;
  pug->steady_cost = 0;
  pug->short_term_risk = _short_term_risk_using_long_term_cache;
  pug->prepare_steady_cost = prepare_steady_cost_static;
  pug->release_steady_cost = release_steady_cost_static;
  return exec;
}

struct exec_t *exec_pug_create_lookback(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_pug_t));
  exec->net_dp = 0;

  exec->validate = _exec_pug_validate;
  exec->run = _exec_pug_runner;
  exec->explain = _exec_pug_long_explain;

  TO_PUG(exec);
  pug->steady_packet_loss = 0;
  pug->steady_cost = 0;
  pug->short_term_risk = _short_term_risk_using_predictor;
  pug->prepare_steady_cost = prepare_steady_cost_dynamic;
  pug->release_steady_cost = release_steady_cost_dynamic;

  return exec;
}
