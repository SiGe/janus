#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

#include "util/log.h"

#include "dataplane.h"
#include "plan.h"
#include "plans/jupiter.h"
#include "risk.h"
#include "traffic.h"

struct expr_t;

struct exec_result_t {
  risk_cost_t   cost;         // Cost of the planner
  unsigned      num_steps;    // Number of steps
  char         *description;  // Description of the change internal
  trace_time_t  at;           // Time step for this result
};

struct exec_output_t {
  struct array_t *result;     // An array_t of exec_result_t
};

/* Exec is the context in which we run the experiments in */
struct exec_t {
  /* Explain what this exec does---this should be presented on the screen in
   * whatever form the exec thinks it's appropriate. */
  void (*explain)  (struct exec_t const *);

  /* Validate and prepare the experiment environment */
  void (*validate) (struct exec_t *, struct expr_t const *expr);

  /* Run the experiment */
  struct exec_output_t * (*run) (struct exec_t *, struct expr_t const *expr);

  struct traffic_matrix_trace_t *trace;
  struct traffic_matrix_trace_t *trace_training;

  /* A list holding network/dataplane instances.
   * This is mainly used in concurrent execution of monte-carlo simulations
   * across different cores.
   *
   * The main reason for having this class is that both dataplane and network
   * are heavy to build and initiate (less so true about dataplane, but network
   * can be rather heavy).  So instead of release and creating new instances
   * we just pass ownership to each simulation instances.
   */
  struct freelist_repo_t *net_dp;
};

/* Traffic stats structures */
struct stats_t {
  bw_t min, max, mean, sum;
};

struct traffic_stats_t {
  struct stats_t in;
  struct stats_t out;
  unsigned pod_id;
};

#define EXEC_VALIDATE_PTR_SET(e, p) {\
  if (e->p == 0) {\
    panic_txt("Need to set "#p);\
  }}

#define EXEC_VALIDATE_STRING_SET(e, p) {\
  if (e->p == 0 || strlen(e->p) == 0) {\
    panic_txt("Need to set string value "#p);\
  }}

#define EXEC_EWMA_PREFIX "traffic"

struct rvar_t **exec_rvar_cache_load(struct expr_t const *expr, unsigned *size);
struct array_t **exec_rvar_cache_load_into_array(struct expr_t const *expr, unsigned *count);

/* Returns or builds the EWMA cache for the expr_t. */
struct predictor_t *exec_ewma_cache_build_or_load(struct exec_t *, struct expr_t const *expr);
struct predictor_t *exec_perfect_cache_build_or_load(struct exec_t *, struct expr_t const *expr);
struct predictor_t *exec_predictor_create(struct exec_t *exec, struct expr_t const *expr, char const *value);

/* Returns the cost of a plan 
 *
 * @expr_t: the setting/config of the experiment.
 * @mops: the list of management operations, aka, the plan.
 * @nmops: the number of mops,
 * @start: the time of the start of the experiment.
 *
 * Should also probably pass other criteria to this to ensure we are within the
 * bound.
 */
risk_cost_t exec_plan_cost(
    struct exec_t *exec, struct expr_t const *expr, struct mop_t **mops,
    uint32_t nmops, trace_time_t start);

rvar_type_t *
exec_simulate_mlu(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);

rvar_type_t *
exec_simulate_ordered(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);

struct rvar_t *
exec_simulate(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);


// TODO: This requires the pod information, that's why we are passing expr.
void exec_traffic_stats(
    struct exec_t const *exec,
    struct expr_t const *expr,
    struct traffic_matrix_trace_iter_t *iter,
    uint32_t ntms,
    struct traffic_stats_t **ret_pod_stats,
    uint32_t *ret_npods,
    struct traffic_stats_t **ret_core_stats);


// Critical path analysis structure
struct exec_critical_path_t {
  struct jupiter_located_switch_t **sws;
  unsigned pod;
  enum   JUPITER_SWITCH_TYPE type;

  // Num switches in this critical path
  unsigned num_switches;

  // Num switches left in the critical path
  unsigned num_switches_left;

  // Steady state bandwidth
  bw_t   bandwidth;

  // Current bandwidth of the critical path
  bw_t   cur_bandwidth;
};

struct exec_critical_path_stats_t {
  struct exec_critical_path_t *paths;
  unsigned num_paths;
};

/* Perform critical path analysis on the remaining set of switches to upgrade:
 *
 * Input to critical path analysis is a traffic trace (with length), the
 * long-term steady state traffic.
 *
 * remaining set of switches to upgrade.  The output is a single step MOp to
 * perform.
 */
struct exec_critical_path_stats_t *exec_critical_path_analysis(
    struct exec_t const *exec, struct expr_t const *expr,
    struct traffic_matrix_trace_iter_t *iter,
    uint32_t iter_length);


struct exec_critical_path_stats_t *exec_critical_path_analysis_online(
    struct exec_t const *exec, struct expr_t const *expr,
    struct traffic_matrix_t **tm, uint32_t ntms);

#endif // 
