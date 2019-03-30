#include <assert.h>
#include <math.h>

#include "algo/array.h"
#include "algo/rvar.h"
#include "plan.h"
#include "util/log.h"

#include "failure.h"

struct rvar_t *failure_default_apply(
    struct failure_model_t const *fm,
    struct network_t *net,
    struct plan_iterator_t *pi,
    struct rvar_t **rcache,
    unsigned subplan_id) {
  struct mop_block_stats_t *blocks = 0;
  struct mop_t *mop = pi->mop_for(pi, subplan_id);
  unsigned nblocks = mop->block_stats(mop, net, &blocks);
  mop->free(mop);

  struct failure_scenario_iterator_t *fi = fm->iter(fm, blocks, nblocks, pi, rcache);
  
  double prob_sum = 0;
  struct array_t *arr_dists = array_create(sizeof(rvar_type_t), 10);
  struct array_t *arr_rvs = array_create(sizeof(struct rvar_t *), 10);
  for (fi->begin(fi); !fi->end(fi); fi->next(fi)) {
    double prob = fi->prob(fi);
    if (prob == 0) continue;

    prob_sum += prob;
    struct rvar_t *rv = fi->cost(fi);

    array_append(arr_rvs, (void *)&rv);
    array_append(arr_dists, (void *)&prob);
  }
  struct rvar_t **rvs = 0;  rvar_type_t *dists = 0;
  unsigned size = array_size(arr_dists);
  array_transfer_ownership(arr_dists, (void**)&dists);
  array_transfer_ownership(arr_rvs, (void**)&rvs);

  // Free resources
  array_free(arr_dists);
  array_free(arr_rvs);
  free(blocks);

  struct rvar_t *ret = rvar_compose_with_distributions(rvs, dists, size);
  fi->free(fi, rvs, (int)size);

  free(dists);
  free(rvs);


  if ((1 - prob_sum) > 0.1) {
    info_txt("Number of concurrent failures to consider is too low---"
        "increase the number of concurrent failures or decrease the"
        "probability of switch failing.");
    char *explanation = pi->explain(pi, subplan_id);
    warn("Explanation: %s", explanation);
    free(explanation);
    panic("Subplan ID: %d, Prob: %f should equal almost 1. Total num scenarios: %d",
        subplan_id, prob_sum, size);
  }
  return ret;
}

