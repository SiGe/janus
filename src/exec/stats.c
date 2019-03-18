#include <assert.h>

#include "util/common.h"
#include "config.h"
#include "network.h"

#include "exec/stats.h"

static int _pod_sort(void const *v1, void const *v2) {
  struct traffic_stats_t *p1 = (struct traffic_stats_t *)v1;
  struct traffic_stats_t *p2 = (struct traffic_stats_t *)v2;

  if (p1->in.max > p2->in.max)      return -1;
  else if (p1->in.max < p2->in.max) return 1;
  else                              return 0;
}

static void _exec_stats_validator(struct exec_t *exec, struct expr_t const *expr) {}

static void _calc_mlu(struct exec_t *exec, struct expr_t *expr) {
  struct traffic_matrix_trace_iter_t *iter = exec->trace->iter(exec->trace);
  int tm_count = iter->length(iter);

  struct traffic_matrix_t **tms = malloc(
      sizeof(struct traffic_matrix_t *) * tm_count);

  int index = 0;
  for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
    struct traffic_matrix_t *tm = 0;
    iter->get(iter, &tm);
    assert(tm != 0);
    tms[index++] = tm;
  }
  iter->free(iter);

  /* The returned rvar_types are in the order they were passed to exec_simulate_ordered */
  rvar_type_t *vals = exec_simulate_mlu(exec, expr, tms, index);

  /* Free the allocated traffic matrices */
  for (uint32_t i = 0; i < tm_count; ++i) {
    traffic_matrix_free(tms[i]);
  }

  rvar_type_t maxm = 0;
  rvar_type_t avgm = 0;
  for (uint32_t i = 0; i < tm_count; ++i) {
    maxm = MAX(maxm, vals[i]);
    avgm += vals[i];
  }
  avgm /= tm_count;
  free(vals);

  info("Max MLU: %f, Avg MLU: %f", maxm, avgm);
}

static struct exec_output_t *
_exec_stats_runner(struct exec_t *exec, struct expr_t *expr) {
  struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(50, expr->traffic_test);
  struct traffic_matrix_trace_iter_t *iter = trace->iter(trace);
  struct traffic_stats_t *pods = 0;
  struct traffic_stats_t *core = 0;
  uint32_t npods = 0;

  //iter->go_to(iter, 1);
  exec_traffic_stats(
      exec, expr, iter, 400, &pods, &npods, &core);

  bw_t core_cap = expr->network->core_capacity(expr->network);
  bw_t pod_cap = expr->network->pod_capacity(expr->network);

  qsort(pods, npods, sizeof(struct traffic_stats_t), _pod_sort);
  info("IN -----");
  for (uint32_t i = 0; i < npods; ++i) {
    info("Pod  [%2d] stats: (%14.2f, %14.2f, %14.2f) (%.2f, %.2f, %.2f)", 
        pods[i].pod_id, pods[i].in.min, pods[i].in.mean, pods[i].in.max, 
        pods[i].in.min/pod_cap, pods[i].in.mean/pod_cap, pods[i].in.max/pod_cap);
  }
  info("Core [  ] stats: (%14.2f, %14.2f, %14.2f) (%.2f, %.2f, %.2f)", 
      core->in.min, core->in.mean, core->in.max,
      core->in.min/core_cap, core->in.mean/core_cap, core->in.max/core_cap);

  info("OUT -----");
  for (uint32_t i = 0; i < npods; ++i) {
    info("Pod  [%2d] stats: (%14.2f, %14.2f, %14.2f) (%.2f, %.2f, %.2f)", 
        pods[i].pod_id, pods[i].out.min, pods[i].out.mean, pods[i].out.max, 
        pods[i].out.min/pod_cap, pods[i].out.mean/pod_cap, pods[i].out.max/pod_cap);
  }
  info("Core [  ] stats: (%14.2f, %14.2f, %14.2f) (%.2f, %.2f, %.2f)", 
      core->out.min, core->out.mean, core->out.max,
      core->out.min/core_cap, core->out.mean/core_cap, core->out.max/core_cap);

  exec->trace = traffic_matrix_trace_load(400, expr->traffic_test);
  _calc_mlu(exec, expr);
  traffic_matrix_trace_free(exec->trace);
  return 0;
}

static void
_exec_stats_explain(struct exec_t *exec) {
  text_block("Stats prints traffic statistics for the pods and core groups.");
}

struct exec_t *exec_traffic_stats_create(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_t));
  exec->net_dp = 0;

  exec->validate = _exec_stats_validator;
  exec->run = _exec_stats_runner;
  exec->explain = _exec_stats_explain;

  return exec;
}
