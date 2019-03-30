#ifndef _FAILURES_JUPITER_WARM_H_
#define _FAILURES_JUPITER_WARM_H_

#include <stdint.h>

#include "failure.h"

/* Warm upgrade failure model for jupiter:
 *
 * The warm upgrade model doesn't actually take down any switch but each switch
 * has a chance of failing.  The planner should shuffle between going with warm
 * and cold reboot depending on failure probability (?)
 *
 * */
struct jupiter_failure_model_warm_t {
  struct failure_model_warm_t;
};

struct jupiter_failure_warm_iterator_t {
  struct failure_scenario_iterator_t;

  /* Internal variable: Number of free switches */
  uint64_t nfreesw;

  /* Internal variable: Number of possible failure states with num_active_failures concurrent switches */
  uint64_t nstates;

  /* Internal variable: ... */
  struct twiddle_t *twiddle;

  /* Internal variable: Current number of switches that have failed concurrently */
  uint64_t num_active_failures;

  /* Internal variable: Failure probability of num_active_failures switches at the same time */
  double failure_prob;
};

/* Creates an warm switch failure model for Jupiter */
struct jupiter_failure_model_warm_t *
  jupiter_failure_model_warm_create(
      unsigned max_conc_failure, 
      double prob,
      double cost);

#endif

