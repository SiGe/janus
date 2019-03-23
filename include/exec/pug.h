#ifndef _EXEC_PUG_H_
#define _EXEC_PUG_H_
#include "exec.h"

struct plan_repo_t {
  unsigned *plans;
  unsigned plan_count;
  unsigned initial_plan_count;
  unsigned max_plan_size;
  unsigned plan_size_in_bytes;
  unsigned cap;

  unsigned _cur_index;
  unsigned _subplan_count;
};

struct exec_pug_t {
  struct exec_t;

  /* Plan repository */
  struct plan_repo_t     *plans;

  /* Plan builder and iterator
   * We initially go through all the plans and add them to the repository of
   * plans so we don't have to iterate through them anymore */
  struct plan_t          *planner;
  struct plan_iterator_t *iter;

  /* Predictor to use if any */
  struct predictor_t *pred;

  /* Long term packet loss and cost random variables */
  struct rvar_t      **steady_packet_loss;
  struct rvar_t      **steady_cost;

  struct rvar_t * (*short_term_risk) (struct exec_t *exec, struct expr_t const *expr, unsigned subplan, trace_time_t);

  void (*prepare_steady_cost) (struct exec_t *exec, struct expr_t const *expr, trace_time_t);
  void (*release_steady_cost) (struct exec_t *exec, struct expr_t const *expr, trace_time_t);
};

struct exec_t *exec_pug_create_short_and_long_term(void);
struct exec_t *exec_pug_create_long_term_only(void);
struct exec_t *exec_pug_create_lookback(void);

#endif // _EXEC_PUG_LONG_H_
