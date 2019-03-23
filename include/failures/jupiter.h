#ifndef _FAILURES_JUPITER_H_
#define _FAILURES_JUPITER_H_

#include <stdint.h>

#include "failure.h"

/* Independent failure model for jupiter */
struct jupiter_failure_model_independent_t {
  struct failure_model_independent_t;
};

struct jupiter_failure_model_independent_t *
  jupiter_failure_model_independent_create(unsigned max_conc_failure, double prob);

#endif

