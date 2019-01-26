#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

#include "dataplane.h"

struct expr_t;

struct exec_t {
  void (*validate) (struct exec_t *, struct expr_t const *expr);
  void (*run)      (struct exec_t *, struct expr_t *expr);

  struct traffic_matrix_trace_t *trace;
  struct freelist_repo_t *net_dp;
};

struct stats_t {
  bw_t min, max, mean, sum;
};

struct traffic_stats_t {
  struct stats_t in;
  struct stats_t out;
  int    id;
};


#include "util/log.h"
#include "plan.h"
#include "risk.h"
#include "traffic.h"

#define EXEC_VALIDATE_PTR_SET(e, p)    { if (e->p == 0) {panic("Need to set "#p);} }
#define EXEC_VALIDATE_STRING_SET(e, p) { if (e->p == 0 || strlen(e->p) == 0) {panic("Need to set string value "#p);} }

#define EXEC_EWMA_PREFIX "traffic"

struct rvar_t **exec_rvar_cache_load(struct expr_t const *expr, int *size);

/* Returns or builds the EWMA cache for the expr_t. */
struct predictor_t *exec_ewma_cache_build_or_load(struct expr_t const *expr);

/* Returns the cost of a plan 
 *
 * expr_t is the setting of the experiment.
 * mops is the list of management operations, aka, the plan.
 * nmops is the number of mops,
 * start is the time of the start of the experiment.
 *
 * Should also probably pass other criteria to this to ensure we are within the
 * bound.
 */
risk_cost_t exec_plan_cost(
    struct exec_t *exec, struct expr_t *expr, struct mop_t **mops,
    uint32_t nmops, trace_time_t start);

struct rvar_t *
exec_simulate(
    struct exec_t *exec,
    struct expr_t *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);


// TODO: This requires the pod information, that's why we are passing expr.
void exec_traffic_stats(
    struct exec_t *exec,
    struct expr_t *expr,
    struct traffic_matrix_trace_iter_t *iter,
    uint32_t ntms,
    struct traffic_stats_t **ret_pod_stats,
    uint32_t *ret_npods,
    struct traffic_stats_t **ret_core_stats);
#endif // 
