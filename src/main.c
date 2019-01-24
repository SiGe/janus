#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "algo/maxmin.h"
#include "config.h"
#include "dataplane.h"
#include "exec/longterm.h"
#include "freelist.h"
#include "network.h"
#include "plan.h"
#include "predictors/ewma.h"
#include "risk.h"
#include "util/common.h"
#include "util/log.h"

const char *usage_message = ""
  "usage: %s <experiment-setting ini file>\n";

void usage(const char *fname) {
  printf(usage_message, fname);
  exit(EXIT_FAILURE);
}

bw_t _dataplane_get_mlu(struct dataplane_t const *dp) {
  bw_t cur_mlu = 0;
  for (int link_id = 0; link_id < dp->num_links; ++link_id) {
    struct link_t *link = &dp->links[link_id];
    bw_t link_util = link->used / link->capacity;
    if (cur_mlu < link_util)
      cur_mlu = link_util;
  }
  return cur_mlu;
}


void _get_violations_mlu_for_dataplane(struct dataplane_t const *dp, int *viol, bw_t *mlu) {
  *viol = dataplane_count_violations(dp, 0);
  *mlu = _dataplane_get_mlu(dp);
}

static void
_simulate_network(struct network_t *network, struct traffic_matrix_t *tm, struct dataplane_t *dp) {
  network->set_traffic(network, tm);
  network->get_dataplane(network, dp);
  maxmin(dp);
}

static void __attribute__((unused))
  test_settings(struct expr_t *expr) {
    struct dataplane_t dp = {0};
    struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(50, expr->traffic_test);
    struct traffic_matrix_trace_iter_t *iter = trace->iter(trace);
    uint32_t tm_idx = 1;

    float  mlu = 0;
    for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
      struct traffic_matrix_t *tm = 0;
      iter->get(iter, &tm);

      _simulate_network(expr->network, tm, &dp);

      bw_t cur_mlu; int violations;
      _get_violations_mlu_for_dataplane(&dp, &violations, &cur_mlu);

      if (violations != 0)
        info("[TM %d] Number of violations: %d", tm_idx, violations);
      tm_idx ++;

      /* Free the traffic matrix */
      traffic_matrix_free(tm);
    }
    info("Maximum network MLU is %f", mlu);

    iter->free(iter);
    traffic_matrix_trace_free(trace);
  }

static void __attribute__((unused))
  test_error_matrices(struct expr_t *expr) {
#define NUM_STEPS 5
    struct dataplane_t dp = {0};
    struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(50, expr->traffic_test);
    struct traffic_matrix_trace_iter_t *iter = trace->iter(trace);

    /* Build a predictor */
    struct predictor_t *pred = (struct predictor_t *)
      predictor_ewma_create(0.8, NUM_STEPS + 1, expr->traffic_training);
    pred->build(pred, trace);

    uint32_t tm_idx = 1;
    bw_t mlu        = 0;

    int prev_viol[NUM_STEPS] = {0};
    bw_t prev_mlu[NUM_STEPS] = {0};
    struct traffic_matrix_t *tms[NUM_STEPS] = {0};

    for (uint32_t i = 0; i < NUM_STEPS; ++i) {
      tms[i] = traffic_matrix_zero(9216);
    }

    int viol_index   = 0;
    trace_time_t time = 0;

    for (iter->begin(iter); !iter->end(iter); iter->next(iter), time += 1) {
      /* Predicted tm matrices */
      struct predictor_iterator_t *piter = pred->predict(
          pred, 
          tms[viol_index], 
          time, time + NUM_STEPS);

      /* Compare prediction results against the real results */
      int steps = 0;
      struct traffic_matrix_t *tm = 0;
      for (piter->begin(piter); !piter->end(piter); piter->next(piter), steps += 1) {
        tm = piter->cur(piter);
        if (tm == 0)
          continue;

        _simulate_network(expr->network, tm, &dp);
        bw_t cur_mlu; int violations;
        _get_violations_mlu_for_dataplane(&dp, &violations, &cur_mlu);

        // compare cur_mlu with the proper index
        int cmp_viol = prev_viol[(NUM_STEPS + viol_index + steps) % NUM_STEPS];
        bw_t cmp_mlu = prev_mlu [(NUM_STEPS + viol_index + steps) % NUM_STEPS];

        info("Prediction for time %d, step %d is %d (pred) vs. %d (real) (%f vs. %f)", 
            time + steps, steps, violations, cmp_viol, cur_mlu, cmp_mlu);
        traffic_matrix_free(tm);
      }

      iter->get(iter, &tm);
      /* Real tm matrices */
      _simulate_network(expr->network, tm, &dp);
      bw_t cur_mlu; int violations;
      _get_violations_mlu_for_dataplane(&dp, &violations, &cur_mlu);

      // Update the violation index
      viol_index += 1;
      if (viol_index >= NUM_STEPS)
        viol_index = 0;

      prev_viol[viol_index] = violations;
      prev_mlu[viol_index] = cur_mlu;

      traffic_matrix_free(tms[viol_index]);
      tms[viol_index] = tm;

      if (violations != 0)
        info("[TM %d] Number of violations: %d", tm_idx, violations);
      tm_idx ++;

      /* Don't forget to free the traffic matrix */
    }
    info("Maximum network MLU is %f", mlu);

    iter->free(iter);
    traffic_matrix_trace_free(trace);
  }

struct _rvar_cache_builder {
  struct network_t *network;
  struct dataplane_t dp;
  struct traffic_matrix_trace_iter_t *iter;
};

rvar_type_t _sim_network_for_trace(void *data) {
  struct _rvar_cache_builder* builder = (struct _rvar_cache_builder*)data;
  struct traffic_matrix_t *tm = 0;

  // Get the next traffic matrix
  builder->iter->get(builder->iter, &tm);

  // Simulate the network
  _simulate_network(builder->network, tm, &builder->dp);

  // Count the violations
  int violations = dataplane_count_violations(&builder->dp, 0);
  rvar_type_t percentage = (rvar_type_t)violations/(rvar_type_t)(builder->network->tm->num_pairs);

  // And free the traffic matrix
  traffic_matrix_free(tm);

  builder->iter->next(builder->iter);
  return percentage;
}

void test_build_rvar_cache(struct expr_t *expr) {
  struct jupiter_switch_plan_enumerator_t *en = 
    jupiter_switch_plan_enumerator_create(
        expr->upgrade_list.num_switches,
        expr->located_switches,
        expr->upgrade_freedom,
        expr->upgrade_nfreedom);

  struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(50, expr->traffic_test);
  struct plan_iterator_t *iter = en->iter((struct plan_t *)en);
  int subplan_count = iter->subplan_count(iter);

  struct _rvar_cache_builder builder = {
    .network = expr->network,
    .dp = {0},
    .iter = trace->iter(trace),
  };

  for (int i = 0; i < subplan_count; ++i) {
    // Apply the mop on the network
    struct mop_t *mop = iter->mop_for(iter, i);
    mop->pre(mop, expr->network);

    builder.iter->begin(builder.iter);
    struct rvar_t *rvar = (struct rvar_t *)rvar_monte_carlo(
        _sim_network_for_trace, 
        400/*trace->num_indices*/, 
        &builder);

    info("Generated rvar for %ith subplan (expected viol: %f)", i, rvar->expected(rvar));

    mop->post(mop, expr->network);
    free(mop);
  }


  dataplane_free_resources(&builder.dp);
  traffic_matrix_trace_free(trace);
  info("Done generating the rvars");
}

void test_planner(struct expr_t *expr) {
  struct jupiter_switch_plan_enumerator_t *en = jupiter_switch_plan_enumerator_create(
      expr->upgrade_list.num_switches,
      expr->located_switches,
      expr->upgrade_freedom,
      expr->upgrade_nfreedom);

  struct plan_iterator_t *iter = en->iter((struct plan_t *)en);

  int *subplans = 0; int subplan_count = 0;
  int tot_plans = 0;

  struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(400, expr->traffic_test);
  struct dataplane_t dp = {0};
  uint32_t count = 0;

  uint32_t seen[125] = {0};

  for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
    iter->plan(iter, &subplans, &subplan_count);

    for (uint32_t id = 0; id < subplan_count; ++id) {
      if (seen[subplans[id]])
        continue;
      seen[subplans[id]] = 1;
      struct mop_t *mop = iter->mop_for(iter, subplans[id]);
      mop->pre(mop, expr->network);

      struct traffic_matrix_trace_iter_t *titer = trace->iter(trace);
      for (titer->begin(titer); !titer->end(titer); titer->next(titer)) {
        struct traffic_matrix_t *tm = 0;
        titer->get(titer, &tm);
        _simulate_network(expr->network, tm, &dp);
        dataplane_count_violations(&dp, 0);
        traffic_matrix_free(tm);
        count += 1;
      }
      mop->post(mop, expr->network);
      mop->free(mop);
    }
    info("Simulated %d plans (%d simulations).", tot_plans, count);

    free(subplans);
    subplans = 0;
    tot_plans += 1;
  }
  iter->free(iter);
  info("Total number of unique plans: %d", tot_plans);
}

struct exec_t *executor(struct expr_t *expr) {
    if (expr->action == BUILD_LONGTERM) {
        return exec_longterm_create();
    }

    panic("Executor not implemented.");
    return 0;
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
  }

  struct expr_t expr = {0};
  config_parse(argv[1], &expr, argc - 1, argv + 1);
  struct exec_t *exec = executor(&expr);

  exec->validate(exec, &expr);
  exec->run(exec, &expr);

  // test_settings(&expr);
  // test_error_matrices(&expr);
  // test_planner(&expr);
  // test_build_rvar_cache(&expr);
  // test_build_rvar_cache_parallel(&expr);

  return EXIT_SUCCESS;
}
