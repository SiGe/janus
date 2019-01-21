#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "algo/maxmin.h"
#include "config.h"
#include "util/common.h"
#include "util/log.h"
#include "network.h"
#include "plan.h"
#include "predictors/ewma.h"
#include "risk.h"

const char *usage_message = ""
  "usage: %s <experiment-setting ini file>\n";

void usage(const char *fname) {
  printf(usage_message, fname);
  exit(EXIT_FAILURE);
}


void _get_violations_mlu_for_dataplane(struct dataplane_t const *dp, int *viol, bw_t *mlu) {
        int violations = 0;
        bw_t cur_mlu = 0;

        for (int flow_id = 0; flow_id < dp->num_flows; ++flow_id) {
            if (dp->flows[flow_id].bw < dp->flows[flow_id].demand) {
                violations +=1 ;
            }
        }

        for (int link_id = 0; link_id < dp->num_links; ++link_id) {
            struct link_t *link = &dp->links[link_id];
            bw_t link_util = link->used / link->capacity;
            if (cur_mlu < link_util)
                cur_mlu = link_util;
        }

        *viol = violations;
        *mlu = cur_mlu;
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

void test_planner(struct expr_t *expr) {
    struct jupiter_switch_plan_enumerator_t *en = jupiter_switch_plan_enumerator_create(
            expr->upgrade_list.num_switches,
            expr->located_switches,
            expr->upgrade_freedom,
            expr->upgrade_nfreedom);

    struct plan_iterator_t *iter = en->iter((struct plan_t *)en);

    int *subplans = 0; int subplan_count = 0;
    int tot_plans = 0;
    int max_id = 0;
    for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
        iter->plan(iter, &subplans, &subplan_count);

        for (uint32_t id = 0; id < subplan_count; ++id) {
            max_id = MAX(max_id, subplans[id]);
        }

        free(subplans);
        subplans = 0;
        tot_plans += 1;
    }
    info("Total number of unique plans: %d (max_id = %d)", tot_plans, max_id);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
  }

  struct expr_t expr = {0};
  config_parse(argv[1], &expr);

  //test_settings(&expr);
  //test_error_matrices(&expr);
  test_planner(&expr);

  return EXIT_SUCCESS;
}
