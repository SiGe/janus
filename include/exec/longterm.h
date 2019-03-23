#ifndef _EXEC_LONGTERM_H_
#define _EXEC_LONGTERM_H_
#include "exec.h"

/*
 * Long term executor generates the packet loss stats for all the traffic
 * matrices in the trace.  The data is used during the simulation and plan
 * selection (for pug-lookback)---both to estimate the "cost" of different
 * plans and as groundtruth and after the fact.  The original pug had to run
 * the simulation for EWMA predictor but pug-lookback can just use the previous
 * data.
 *
 * TODO: It should do so for both the test and training trace, but right now
 * they are both the same because pug-long and pug-lookback don't depend on it.
 */
struct exec_longterm_t {
  struct exec_t;
};

struct exec_t *exec_longterm_create(void);

#endif // _EXEC_LONGTERM_H_
