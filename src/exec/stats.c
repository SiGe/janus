#include "config.h"
#include "exec/stats.h"

int _pod_sort(void const *v1, void const *v2) {
  struct traffic_stats_t *p1 = (struct traffic_stats_t *)v1;
  struct traffic_stats_t *p2 = (struct traffic_stats_t *)v2;

  if (p1->in.max > p2->in.max)      return -1;
  else if (p1->in.max < p2->in.max) return 1;
  else                              return 0;
}

void _exec_stats_validator(struct exec_t *exec, struct expr_t const *expr) {}

void _exec_stats_runner(struct exec_t *exec, struct expr_t *expr) {
  struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(50, expr->traffic_test);
  struct traffic_matrix_trace_iter_t *iter = trace->iter(trace);
  struct traffic_stats_t *pods = 0;
  struct traffic_stats_t *core = 0;
  uint32_t npods = 0;

  //iter->go_to(iter, 1);
  exec_traffic_stats(
      exec, expr, iter, 400, &pods, &npods, &core);

  qsort(pods, npods, sizeof(struct traffic_stats_t), _pod_sort);
  info("IN -----");
  for (uint32_t i = 0; i < npods; ++i) {
    info("Pod  [%2d] stats: (%14.2f, %14.2f, %14.2f)", pods[i].pod_id, pods[i].in.min, pods[i].in.mean, pods[i].in.max);
  }
  info("Core [  ] stats: (%14.2f, %14.2f, %14.2f)", core->in.min, core->in.mean, core->in.max);

  info("OUT -----");
  for (uint32_t i = 0; i < npods; ++i) {
    info("Pod  [%2d] stats: (%14.2f, %14.2f, %14.2f)", pods[i].pod_id, pods[i].out.min, pods[i].out.mean, pods[i].out.max);
  }
  info("Core [  ] stats: (%14.2f, %14.2f, %14.2f)", core->out.min, core->out.mean, core->out.max);
}

struct exec_t *exec_traffic_stats_create(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_t));

  exec->net_dp = 0;
  exec->validate = _exec_stats_validator;
  exec->run = _exec_stats_runner;

  return exec;
}
