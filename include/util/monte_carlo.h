#ifndef _UTIL_MONTE_CARLO_H_
#define _UTIL_MONTE_CARLO_H_

#include "algo/rvar.h"

typedef rvar_type_t (*monte_carlo_run_t)(void *);
typedef void (*monte_carlo_run_multi_t)(void *, rvar_type_t **, int);

// Monte carlo methods for keeping single or multiple RVs
struct rvar_sample_t *monte_carlo_rvar(
    monte_carlo_run_t run,
    int nsteps,
    void *data);

struct rvar_sample_t **monte_carlo_multi_rvar(
    monte_carlo_run_multi_t run,
    int nsteps, int nvars,
    void *data);

struct rvar_sample_t *monte_carlo_parallel_rvar(
    monte_carlo_run_t run, // Monte carlo runner
    void *data,            // Data to pass to each instance of monte-carlo run (this should be an array of size nsteps)
    int nsteps,            // Amount of data
    int size,              // Size of each data segment
    int num_threads        // Number of thread to use or 0 for automatic calculation
);

rvar_type_t *monte_carlo_parallel_ordered_rvar(
    monte_carlo_run_t run, // Monte carlo runner
    void *data,            // Data to pass to each instance of monte-carlo run (this should be an array of size nsteps)
    int nsteps,            // Amount of data
    int size,              // Size of each data segment
    int num_threads        // Number of thread to use or 0 for automatic calculation
);


#endif
