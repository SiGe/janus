#ifndef _EXEC_PUG_H_
#define _EXEC_PUG_H_
#include "exec.h"

struct plan_repo_t;

/*
 * Ah the good ol' pug (or Janus or LaLa).
 *
 * Pug has three modes of operation:
 *
 * - pug: which uses predictions of short-term traffic together with long-term
 *   predictions (pug-long) to plan.
 * - pug-long: which plans based on the full history of the traffic.
 * - pug-lookback: which uses the most n-recent TMs as an estimation of the
 *   long-term trend of traffic.  For the short-term prediction it operates
 *   similar to pug.
 *
 * pug seems to be having problem in general because the pug-long predictions
 * can be way off at times.  pug-lookback works better than pug due to this
 * exact reason.  That long-term predictions are more sensible.
 */
struct exec_pug_t {
  struct exec_t;

  /* Plan repository */
  struct plan_repo_t     *plans;

  /* Plan builder and iterator objects.  We initially go through all the plans
   * and add them to the repository of plans so we don't have to iterate through
   * them anymore */
  struct plan_t          *planner;
  struct plan_iterator_t *iter;

  /* Predictor to use if any */
  struct predictor_t *pred;

  /* Long term packet loss random variables per subplan */
  struct rvar_t      **steady_packet_loss;
  /* Long term cost random variables per subplan */
  struct rvar_t      **steady_cost;

  /* Short term risk computation function for subplan at trace_time_t */
  struct rvar_t * (*short_term_risk) (struct exec_t *exec, struct expr_t const *expr, unsigned subplan, trace_time_t);

  /* Prepares the steady cost (i.e., long term costs) at step trace_time_t.
   * For, pug-lookback this translates to the last n-steps.  For pug-long and
   * pug, this translates to the whole history of the traffic. */
  void (*prepare_steady_cost) (struct exec_t *exec, struct expr_t const *expr, trace_time_t);
  void (*release_steady_cost) (struct exec_t *exec, struct expr_t const *expr, trace_time_t);
};

/* TODO: Probably can add a factory method for generating different pug planners
 * or to mix and match them, eh ... maybe later.
 *
 * - Omid 3/23/2019
 */
/* pug */
struct exec_t *exec_pug_create_short_and_long_term(void);
/* pug-long */
struct exec_t *exec_pug_create_long_term_only(void);
/* pug-lookback */
struct exec_t *exec_pug_create_lookback(void);

/*
 * A plan repository of possible plans of length max_plan_size for pug.
 *
 * Internally pug uses _plan_invalidate_not_equal, _cur_index, and plan_count to
 * splice the space of possible subplans by subplans we have already chosen.
 *
 * E.g., if our choices of plans are:
 *
 * [0 , 1, 3]
 * [10, 1, 4]
 * [10, 2, 5]
 *
 * and we choose subplan 10 as our first step, we can invalidate plans that do
 * not start with 10.  So our choices are limited to:
 *
 * [10, 1, 4]
 * [10, 2, 5]
 *
 * To make restoration of plan_repo_t easier (so we can do this continously at
 * each step and won't need to regenerate it) we just swap invalidate plan with
 * an element that is valid, but adjust plan_count accordingly to make sure we
 * don't iterate over that plan.  In the previous example:
 * 
 * [10, 1, 4]
 * [10, 2, 5]
 * [0 , 1, 3]
 *
 * plan_count = 2, initial_plan_count = 3
 *
 * If we want to "reset" the planner, we just set the plan_count to 3 and all
 * plans become valid again.
 */
struct plan_repo_t {
  unsigned *plans;              /* An array containing all plans */
  unsigned plan_count;          /* Number of plans, i.e., the length of *plans array */
  unsigned initial_plan_count;  /* We use this to reset the plan repository back to its original state */
  unsigned max_plan_size;       /* Max length of a plan in this repository---this is equivalent to the deadline? */
  unsigned plan_size_in_bytes;  /* Length of the plan in bytes, i.e., max_plan_size * sizeof(unsigned) */
  unsigned cap;                 /* Capacity of the planner, i.e., how many plans it can hold */

  /* Internal variables for working with subplans */
  unsigned _cur_index;          /* Index of last subplan "invalidated" in the
                                 * plan_repo_t.  Look at the _plan_invalidate_not_equal */
  unsigned _subplan_count;      /* Number of possible subplans---this is read
                                   from the freedom degree */
};


#endif // _EXEC_PUG_LONG_H_
