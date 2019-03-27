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

/* TODO: I can probably change this to blocks.  mop_block_stats_t and
 * mop_block_id_t.
 *
 * - Omid 3/26/2019
 * */
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

/* Loads the rvar cache (long-term data) specified in the expr */
struct rvar_t **exec_rvar_cache_load(struct expr_t const *expr, unsigned *size);

/* Loads the long-term data cache, built using -a long-term, specified in the
 * expr.  The cached data is in-order so i'th entry maps to the i'th traffic
 * matrix in the trace.  */
struct array_t **exec_rvar_cache_load_into_array(struct expr_t const *expr, unsigned *count);

/* Returns or builds the EWMA cache for the expr_t. */
/* TODO: Useless for now.  The EWMA predictor is pretty lackluster */
struct predictor_t *exec_ewma_cache_build_or_load(struct exec_t *, struct expr_t const *expr);

/* Load the perfect predictor data into the cache. */
/* TODO: Does nothing right now, maybe I can remove it? */
struct predictor_t *exec_perfect_cache_build_or_load(struct exec_t *, struct expr_t const *expr);

/* Creates a predictor depending on the value passed.
 * TODO: This is different than the way we create/load the rvar_cache.  Is this
 * really ok?  This accepts a string (which should be available? in expr) and
 * outputs a predictor.  Whereas the rvar_cache gets the string from the expr
 * */
struct predictor_t *exec_predictor_create(struct exec_t *exec, struct expr_t const *expr, char const *value);

/* Returns the cost of a plan 
 *
 * expr_t: the setting/config of the experiment.
 * mops: the list of management operations, aka, the plan.
 * nmops: the number of mops,
 * start: the time of the start of the experiment.
 *
 * Should also probably pass other criteria to this to ensure we are within the
 * criteria bounds?
 */
risk_cost_t exec_plan_cost(
    struct exec_t *exec, struct expr_t const *expr, struct mop_t **mops,
    uint32_t nmops, trace_time_t start);

/* Returns the MLU of the network for each interval */
rvar_type_t *
exec_simulate_mlu(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);

/* Simulates the network and uses the dataplane_count_violations to count the
 * number of violations in the network in each interval */
rvar_type_t *
exec_simulate_ordered(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);

/* Similar to simulate_ordered but returns a random variable */
struct rvar_t *
exec_simulate(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length);


/* Returns traffic stats of each pods */
// TODO: This requires the pod information, that's why we are passing expr.
// Probably the more interesting fact about the stats is to return the BLOCK
// information, not core and pods.
void exec_traffic_stats(
    struct exec_t const *exec,
    struct expr_t const *expr,
    struct traffic_matrix_trace_iter_t *iter,
    uint32_t ntms,
    struct traffic_stats_t **ret_pod_stats,
    uint32_t *ret_npods,
    struct traffic_stats_t **ret_core_stats);


/* Critical path analysis data structure */
struct exec_critical_path_t {
  /* List of located switches in this critical path */
  struct jupiter_located_switch_t **sws;

  /* Pod id for this critical path */
  unsigned pod;

  /* Type of the switches in this critical path */
  enum   JUPITER_SWITCH_TYPE type;

  /* Num switches in this critical path (i.e., pod/core group in Jupiter topology) */
  unsigned num_switches;

  /* Num switches left in the critical path (i.e., num switches not touched in a
   * pod/core group in Jupiter topology */
  unsigned num_switches_left;

  /* Steady state bandwidth */
  bw_t   bandwidth;

  /* Current bandwidth of the critical path */
  bw_t   cur_bandwidth;
};

struct exec_critical_path_stats_t {
  struct exec_critical_path_t *paths;

  /* Number of paths, aka, number of pods/cores getting upgraded in jupiter
   * topology */
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
