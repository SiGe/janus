#include <stdlib.h>

#include "thpool/thpool.h"
#include "algo/rvar.h"
#include "util/common.h"

#include "util/monte_carlo.h"

struct rvar_sample_t *monte_carlo_rvar(
        rvar_type_t (*single_run)(void *data),
        int nsteps, void *data) {
    struct rvar_sample_t *ret = (struct rvar_sample_t *)rvar_sample_create(nsteps);

    for (int i = 0; i < nsteps; ++i) {
        ret->vals[i] = single_run(data);
    }

    /* TODO: This is so stupid, we shouldn't need to do this */
    rvar_sample_finalize(ret, nsteps);

    return ret;
}

struct rvar_sample_t **monte_carlo_multi_rvar(
        monte_carlo_run_multi_t run,
        int nsteps, int nvars, void *data) {
    struct rvar_sample_t **vars = malloc(sizeof(struct rvar_sample_t*) * nvars);
    rvar_type_t **ptrs = malloc(sizeof(rvar_type_t *) * nvars);

    for (int i = 0; i < nvars; ++i) {
      vars[i] = (struct rvar_sample_t *)rvar_sample_create(nsteps);
      ptrs[i] = vars[i]->vals;
    }


    for (int i = 0; i < nsteps; ++i) {
        run(data, ptrs, nvars);

        for (int j = 0; j < nvars; ++j)
            ptrs[j]++;
    }

    /* TODO: This is so stupid, we shouldn't need to do this */
    for (int i = 0; i < nvars; ++i) {
      rvar_sample_finalize(vars[i], nsteps);
    }

    free(ptrs);
    return vars;
}

struct _monte_carlo_parallel_t {
  void *data;
  int  index;
  rvar_type_t *vals;
  monte_carlo_run_t runner;
};

static void _mcpd_rvar_runner(void *data) {
  struct _monte_carlo_parallel_t *mpcd = (struct _monte_carlo_parallel_t *)data;
  rvar_type_t ret = mpcd->runner(mpcd->data);
  mpcd->vals[mpcd->index] = ret;
}

rvar_type_t *monte_carlo_parallel_ordered_rvar(
    monte_carlo_run_t run,
    void *data,
    int nsteps,
    int dsize,
    int num_threads
) {
  if (num_threads == 0)
    num_threads = get_ncores() - 1;
    if (num_threads == 0)
      num_threads = 1;

  threadpool thpool = thpool_init(num_threads);

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

  return vals;
}


struct rvar_sample_t *monte_carlo_parallel_rvar(
    monte_carlo_run_t run,
    void *data,
    int nsteps,
    int dsize,
    int num_threads) {
  rvar_type_t *vals = monte_carlo_parallel_ordered_rvar(run, data, nsteps, dsize, num_threads);
  struct rvar_sample_t *rv = (struct rvar_sample_t *)rvar_sample_create_with_vals(vals, nsteps);
  rvar_sample_finalize(rv, nsteps);

  return rv;
}
