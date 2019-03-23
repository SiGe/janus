#include <stdlib.h>

#include "thpool/thpool.h"

#include "algo/array.h"
#include "algo/rvar.h"
#include "util/common.h"
#include "util/monte_carlo.h"

struct rvar_sample_t *monte_carlo_rvar(
    rvar_type_t (*single_run)(void *data),
    unsigned nsteps, void *data) {
  struct array_t *arr = array_create(sizeof(rvar_type_t), nsteps);
  for (unsigned i = 0; i < nsteps; ++i) {
    rvar_type_t val = single_run(data);
    array_append(arr, &val);
  }

  rvar_type_t *vals = 0;
  array_transfer_ownership(arr, (void**)&vals);
  array_free(arr);

  return (struct rvar_sample_t *)rvar_sample_create_with_vals(vals, nsteps);
}

struct _monte_carlo_parallel_t {
  void *data;
  unsigned index;
  rvar_type_t *vals;
  monte_carlo_run_t runner;
};

static void _mcpd_rvar_runner(void *data) {
  struct _monte_carlo_parallel_t *mpcd = (struct _monte_carlo_parallel_t *)data;
  rvar_type_t ret = mpcd->runner(mpcd->data);
  mpcd->vals[mpcd->index] = ret;
}

rvar_type_t *monte_carlo_parallel_ordered_rvar(
    monte_carlo_run_t run, void *data,
    unsigned nsteps, unsigned dsize, unsigned num_threads
) {
  if (num_threads == 0) {
    num_threads = get_ncores() - 1;
    if (num_threads == 0)
      num_threads = 1;
  }

  threadpool thpool = thpool_init((int)num_threads);

  struct _monte_carlo_parallel_t *mcpd = malloc(sizeof(struct _monte_carlo_parallel_t) * nsteps);
  rvar_type_t *vals = malloc(sizeof(rvar_type_t) * nsteps);

  for (uint32_t i = 0; i < nsteps; ++i) {
    mcpd[i].data = ((char *)data) + (dsize * i);
    mcpd[i].index = i;
    mcpd[i].vals = vals;
    mcpd[i].runner = run;
  }

  for (uint32_t i = 0; i < nsteps; ++i) {
    thpool_add_work(thpool, _mcpd_rvar_runner, &mcpd[i]);
  }

  thpool_wait(thpool);
  thpool_destroy(thpool);
  free(mcpd);

  return vals;
}

struct rvar_sample_t *monte_carlo_parallel_rvar(
    monte_carlo_run_t run, void *data,
    unsigned nsteps, unsigned dsize, unsigned num_threads) {
  rvar_type_t *vals = monte_carlo_parallel_ordered_rvar(run, data, nsteps, dsize, num_threads);

  struct rvar_sample_t *rv = (struct rvar_sample_t *)rvar_sample_create_with_vals(vals, nsteps);
  return rv;
}
