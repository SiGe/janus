#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

struct expr_t;

struct exec_t {
  void (*validate) (struct exec_t *, struct expr_t const *expr);
  void (*run)      (struct exec_t *, struct expr_t *expr);

  struct freelist_repo_t *net_dp;
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
risk_cost_t plan_cost(
    struct expr_t *expr, struct mop_t **mops,
    uint32_t nmops, trace_time_t start);

struct rvar_t *
exec_simulate(
    struct exec_t *exec,
    struct expr_t *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);

#endif // 
