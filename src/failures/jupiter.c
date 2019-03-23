#include <assert.h>
#include <math.h>

#include "algo/array.h"
#include "algo/group_gen.h"
#include "algo/rvar.h"
#include "config.h"
#include "plan.h"
#include "twiddle/twiddle.h"
#include "util/log.h"

#include "failures/jupiter.h"

static inline
uint64_t __attribute__((unused)) choose(uint64_t n, uint64_t k) {
  uint64_t ret = 1;
  // Speed up : C(n k) = C(n n-k)
  if (k > n - k) {
    k = n - k;
  }

  for (uint64_t i = 1; i <= k; ++i) {
    ret *= (n - (k - i))/(i); 
  }

  return ret;
}

double _prob_for_failure(unsigned *tuple, struct mop_block_stats_t *blocks, 
    unsigned nblocks, uint64_t nstates) {
  double prob = 1;

  for (int i = 0; i < nblocks; ++i) {
    uint64_t free_sw = blocks[i].all_switches - blocks[i].down_switches;
    if (tuple[i] > free_sw)
      return 0;

    prob *= (double)choose(free_sw, tuple[i]);
  }

  prob /= (double)nstates;
  return prob;
}

// Returns the subplan id of the least dominative subplan
uint32_t _lds_for_failure(unsigned *tuple, struct mop_block_stats_t *blocks, 
    unsigned nblocks, struct plan_iterator_t *iter) {
  // Update block stats to reflect the failure
  for (int i = 0; i < nblocks; ++i) {
    blocks[i].down_switches += tuple[i];
  }

  uint32_t ret = iter->least_dominative_subplan(iter, blocks, nblocks);

  for (int i = 0; i < nblocks; ++i) {
    assert(blocks[i].down_switches >= tuple[i]);
    blocks[i].down_switches -= tuple[i];
  }

  return ret;
}

struct rvar_t *_jupiter_independent_apply(
    struct failure_model_t const *fm,
    struct network_t *net,
    struct plan_iterator_t *pi,
    struct rvar_t **rcache,
    unsigned subplan_id) {
  struct jupiter_failure_model_independent_t *jfi = 
    (struct jupiter_failure_model_independent_t *)fm;
  unsigned mcsf = jfi->max_concurrent_switch_failure;
  double sfp = jfi->switch_failure_probability;

  //struct rvar_t *base = rcache[subplan_id];

  struct mop_block_stats_t *blocks = 0;

  struct mop_t *mop = pi->mop_for(pi, subplan_id);
  unsigned nblocks = mop->block_stats(mop, net, &blocks);
  mop->free(mop);

  // Count the number of switches that can fail "concurrently"
  uint64_t nfreesw = 0;
  for (int i = 0; i < nblocks; ++i) {
    assert(blocks[i].all_switches >= blocks[i].down_switches);
    nfreesw += (blocks[i].all_switches - blocks[i].down_switches);
  }

  struct array_t *arr_dists = array_create(sizeof(rvar_type_t), 10);
  struct array_t *arr_rvs = array_create(sizeof(struct rvar_t *), 10);

  double prob_sum = 0;
  for (unsigned i = 0; i <= mcsf; ++i) {
    // Model i failure over nblocks
    struct twiddle_t *t = twiddle_create((int)i, (int)nblocks);
    uint64_t nstates = choose(nfreesw, i);
    double prob_i_failure = nstates * pow(sfp, i) * pow(1-sfp, nfreesw - i);

    for (t->begin(t); !t->end(t); t->next(t)) {
      unsigned *tuple = t->tuple(t);

      // Probability of failure under scenario
      double prob = _prob_for_failure(tuple, blocks, nblocks, nstates);

      // Get the actual probability of this scenario happening
      prob *= prob_i_failure;
      prob_sum += prob;

      uint32_t lds = _lds_for_failure(tuple, blocks, nblocks, pi);

      // Get the random variable associated with this state
      // info("Prob failure: %f with %d switches -- subplan id is: %u", prob, i, lds);

      struct rvar_t *rv = rcache[lds];

      array_append(arr_rvs, (void *)&rv);
      array_append(arr_dists, (void *)&prob);
    }

    t->free(t);
  };

  struct rvar_t **rvs = 0;  rvar_type_t *dists = 0;
  unsigned size = array_size(arr_dists);
  array_transfer_ownership(arr_dists, (void**)&dists);
  array_transfer_ownership(arr_rvs, (void**)&rvs);

  // Free resources
  array_free(arr_dists);
  array_free(arr_rvs);
  free(blocks);

  struct rvar_t *ret = rvar_compose_with_distributions(rvs, dists, size);
  free(dists);
  free(rvs);

  if ((1 - prob_sum) > 0.1) {
    info_txt("Number of concurrent failures to consider is too low --- increase the"
        "number of concurrent failures or decrease the probability of switch"
        "failing.");
    panic("Prob total: %d, %f should equal almost 1. Total num scenarios: %d", subplan_id, prob_sum, size);
  }
  return ret;
}

struct jupiter_failure_model_independent_t *
jupiter_failure_model_independent_create (uint32_t max_concurrent_failure,
    double switch_failure_probability) {
  struct jupiter_failure_model_independent_t *ret = 
    malloc(sizeof(struct jupiter_failure_model_independent_t));

  ret->apply = _jupiter_independent_apply;
  ret->max_concurrent_switch_failure = max_concurrent_failure;
  ret->switch_failure_probability = switch_failure_probability;
  return ret;
}
