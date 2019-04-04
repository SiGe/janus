#ifndef _RISK_H_
#define _RISK_H_

#include <stdint.h>
#include "algo/rvar.h"

/* Functions for estimating the risk of a plan.
 *
 * The "risk" depends on the:
 *    - Traffic prediction model
 *    - Data plane model
 *    - Risk "models"
 *
 * Risk is a function that accepts a plan state, measures some property of
 * it, and translate it to a single number indicating what the cost of that
 * dataplane is.
 *
 * A plan state is:
 *
 *  ToR demands (SLO violation factors?)
 *  ToR bandwidths
 *  Link capacities (utilization factors?)
 *  Link bandwidths
 *  Plan start/cur/end (slowness factor)
 *
 */

typedef double risk_cost_t;
struct risk_cost_func_t;

//typedef risk_cost_t (*risk_func_t)(struct rvar_t const *state);
typedef risk_cost_t (*risk_func_t)(struct risk_cost_func_t*, rvar_type_t val);

struct criteria_time_t {
  int         (*acceptable) (struct criteria_time_t *, uint32_t length);

  // Cost of finishing the step in step steps
  risk_cost_t (*cost)       (struct criteria_time_t *, unsigned step);

  unsigned steps;
  risk_cost_t *steps_cost;
};

struct risk_cost_func_t {
  risk_func_t cost;
  risk_cost_t (*rvar_to_cost)(struct risk_cost_func_t *f, struct rvar_t *rvar);
  struct rvar_t * (*rvar_to_rvar)(struct risk_cost_func_t *f, struct rvar_t *rvar, rvar_type_t);
};

struct _rcf_pair_t {
  rvar_type_t step;
  risk_cost_t cost;
};

struct risk_cost_func_step_t {
  struct risk_cost_func_t;
  struct _rcf_pair_t *pairs;
  unsigned nsteps;
};

struct risk_cost_func_linear_t {
  struct risk_cost_func_t;
  double slope;
  double round;
  double max;
};

struct risk_cost_func_poly_t {
  struct risk_cost_func_t;
  double power;
  double ratio;
  double round;
  double max;
};

struct risk_cost_func_exponential_t {
  struct risk_cost_func_t;
  double power;
  double ratio;
  double round;
  double max;
};

struct risk_cost_func_logarithmic_t {
  struct risk_cost_func_t;
  double power;
  double ratio;
  double round;
  double max;
};

struct risk_cost_func_t *
risk_cost_string_to_func(char const *func);

typedef int (*criteria_length_t)(unsigned, unsigned);

#endif // _RISK_H_
