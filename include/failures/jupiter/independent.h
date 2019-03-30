#ifndef _FAILURES_JUPITER_INDEPENDENT_H_
#define _FAILURES_JUPITER_INDEPENDENT_H_

#include <stdint.h>

#include "failure.h"

/* Independent failure model for jupiter:
 *
 * The independent failure model randomly fails any switch with equal
 * probability---there are no correlated failures.  We use automorphism of the
 * failure space to reduce the number of states that we need to explore.  
 * */
struct jupiter_failure_model_independent_t {
  struct failure_model_independent_t;
};

struct jupiter_failure_independent_iterator_t {
  struct failure_scenario_iterator_t;

  /* Number of free switches */
  uint64_t nfreesw;

  /* Number of possible failure states with num_active_failures concurrent switches */
  uint64_t nstates;

  /* ... */
  struct twiddle_t *twiddle;

  /* Current number of switches that have failed concurrently */
  uint64_t num_active_failures;

  /* Failure probability of num_active_failures switches at the same time */
  double failure_prob;
};


/* Creates an independent switch failure model for Jupiter */
struct jupiter_failure_model_independent_t *
  jupiter_failure_model_independent_create(unsigned max_conc_failure, double prob);

#endif

