#include <assert.h>
#include <math.h>

#include "algo/array.h"
#include "algo/group_gen.h"
#include "algo/rvar.h"
#include "config.h"
#include "plan.h"
#include "twiddle/twiddle.h"
#include "util/common.h"
#include "util/log.h"

#include "failures/jupiter/independent.h"

static double _prob_for_failure(unsigned *tuple, struct mop_block_stats_t *blocks, 
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

static double _failure_prob_of_k_concurrent_sws(
    struct jupiter_failure_model_independent_t *jm,
    struct jupiter_failure_independent_iterator_t *ji) {
  uint64_t nfreesw = ji->nfreesw;
  uint64_t naf = ji->num_active_failures;
  double sfp = jm->switch_failure_probability;
  uint64_t nstates = choose(nfreesw, naf);

  /* Overall failure probability of num_active_failures concurrent switches */
  return nstates * pow(sfp, naf) * pow(1-sfp, nfreesw - naf);
}

// Returns the subplan id of the least dominative subplan
static uint32_t _lds_for_failure(unsigned *tuple, struct mop_block_stats_t *blocks, 
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

#define TO_JI(p) struct jupiter_failure_independent_iterator_t *ji =\
  (struct jupiter_failure_independent_iterator_t *)(p)

#define TO_JM(p) struct jupiter_failure_model_independent_t *jm =\
  (struct jupiter_failure_model_independent_t*)(p)

static void _jii_begin(struct failure_scenario_iterator_t *fi) {
  TO_JI(fi);
  TO_JM(fi->model);

  struct mop_block_stats_t *blocks = ji->blocks;
  int nblocks = (int)ji->nblocks;

  // Count the number of switches that can fail "concurrently"
  uint64_t nfreesw = 0;
  for (int i = 0; i < nblocks; ++i) {
    assert(blocks[i].all_switches >= blocks[i].down_switches);
    nfreesw += (blocks[i].all_switches - blocks[i].down_switches);
  }

  ji->nfreesw = nfreesw;
  ji->num_active_failures = 0;
  ji->nstates = choose(nfreesw, 0);
  
  // Free the twiddler if it exists
  if (ji->twiddle)
    ji->twiddle->free(ji->twiddle);

  ji->twiddle = twiddle_create((int)ji->num_active_failures, (int)nblocks);
  ji->twiddle->begin(ji->twiddle);
  ji->failure_prob = _failure_prob_of_k_concurrent_sws(jm, ji);
}

static void _jii_next(struct failure_scenario_iterator_t *fi) {
  TO_JI(fi);
  TO_JM(fi->model);
  // If we are done, do nothing
  if (ji->num_active_failures > jm->max_concurrent_switch_failure)
    return;

  struct twiddle_t *t = ji->twiddle;
  t->next(t);

  // If twiddle hasn't exhausted yet continue.
  if (!t->end(t))
    return;

  // Else increase the number of active failures
  ji->num_active_failures += 1;
  if (ji->num_active_failures > jm->max_concurrent_switch_failure)
    return;

  // Update the failure probablity of k concurrent sws
  ji->failure_prob = _failure_prob_of_k_concurrent_sws(jm, ji);
  ji->nstates = choose(ji->nfreesw, ji->num_active_failures);

  // Free and create a new twiddle
  ji->twiddle->free(ji->twiddle);
  ji->twiddle = twiddle_create((int)ji->num_active_failures, (int)ji->nblocks);
  ji->twiddle->begin(ji->twiddle);
}

static int _jii_end(struct failure_scenario_iterator_t *fi) {
  TO_JI(fi);
  TO_JM(fi->model);

  return (ji->num_active_failures > jm->max_concurrent_switch_failure);
}

static void _jii_free(struct failure_scenario_iterator_t *fi,
    struct rvar_t __attribute__((unused)) **rvs, 
    int __attribute__((unused)) nvars) {
  TO_JI(fi);
  if (ji->twiddle)
    ji->twiddle->free(ji->twiddle);
  free(fi);
}

static rvar_type_t _jii_prob(struct failure_scenario_iterator_t *fi){
  TO_JI(fi);
  unsigned *tuple = ji->twiddle->tuple(ji->twiddle);

  return ji->failure_prob * 
    _prob_for_failure(tuple, ji->blocks, ji->nblocks, ji->nstates);
}

static struct rvar_t *_jii_cost(struct failure_scenario_iterator_t *fi) {
  TO_JI(fi);
  unsigned *tuple = ji->twiddle->tuple(ji->twiddle);
  unsigned lds_id = _lds_for_failure(tuple, ji->blocks, ji->nblocks, ji->pi);

  return ji->rcache[lds_id];
}

static struct failure_scenario_iterator_t *_jupiter_independent_iter(
    struct failure_model_t const *fm,
    struct mop_block_stats_t *blocks,
    uint32_t nblocks,
    struct plan_iterator_t *pi,
    struct rvar_t **rcache) {
  size_t size = sizeof(struct jupiter_failure_independent_iterator_t);
  struct jupiter_failure_independent_iterator_t *iter = malloc(size);
  memset(iter, 0, size);

  iter->blocks = blocks;
  iter->nblocks = nblocks;
  iter->pi = pi;
  iter->rcache = rcache;
  iter->model = fm;

  iter->begin = _jii_begin;
  iter->next = _jii_next;
  iter->end = _jii_end;
  iter->prob = _jii_prob;
  iter->cost = _jii_cost;
  iter->free = _jii_free;

  return (struct failure_scenario_iterator_t *)iter;
}

struct jupiter_failure_model_independent_t *
jupiter_failure_model_independent_create (uint32_t max_concurrent_failure,
    double switch_failure_probability) {
  struct jupiter_failure_model_independent_t *ret = 
    malloc(sizeof(struct jupiter_failure_model_independent_t));

  ret->apply = failure_default_apply;
  ret->iter = _jupiter_independent_iter;
  ret->max_concurrent_switch_failure = max_concurrent_failure;
  ret->switch_failure_probability = switch_failure_probability;
  return ret;
}
