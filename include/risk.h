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

typedef struct rvar_t* (*risk_func_t)(struct rvar_t const *state);

/* Empty interface for now (?) Maybe change later for serialization/memoization? */
struct risk_t {
};

#endif // _RISK_H_
