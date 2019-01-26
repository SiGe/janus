#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/common.h"
#include "util/log.h"

#include "risk.h"

risk_cost_t _default_rvar_to_cost (struct risk_cost_func_t *f, struct rvar_t *rvar) {
  risk_cost_t out = 0;
  struct rvar_t *ret = 0;

  if (rvar->_type == SAMPLED) {
    /* If sampled ... do value to value translation */
    struct rvar_sample_t *rs = (struct rvar_sample_t *)rvar;
    ret = rvar_sample_create(rs->num_samples);
    struct rvar_sample_t *rss = (struct rvar_sample_t *)ret;

    for (uint32_t i = 0; i < rs->num_samples; ++i) {
      rss->vals[i] = f->cost(f, rs->vals[i]);
    }
  } else if (rvar->_type == BUCKETED){
    /* If bucket, create a new bucket variable where low = min(cost) and num buckets = (max(cost) - min(cost))/100?? */
    struct rvar_bucket_t *rs = (struct rvar_bucket_t *)rvar;
    rvar_type_t low = INFINITY;
    rvar_type_t high = -INFINITY;
    rvar_type_t bucket = rs->low;

    for (uint32_t i = 0; i < rs->nbuckets; ++i) {
      if (rs->buckets[i] != 0) {
        risk_cost_t bucket_cost =  f->cost(f, bucket);
        high = MAX(high, bucket_cost);
        low = MIN(low, bucket_cost);
        bucket += rs->bucket_size;
      }
    }

    int nbuckets = 1000;
    if (low == high)
      high = low + 1;
    rvar_type_t bucket_size = (high - low) / nbuckets;
    ret = rvar_bucket_create(low, bucket_size, nbuckets);
    bucket = rs->low;
    for (uint32_t i = 0; i < rs->nbuckets; ++i) {
      if (rs->buckets[i] != 0) {
        risk_cost_t bucket_cost =  f->cost(f, bucket);
        int bidx = (int)((bucket_cost - low) / bucket_size);
        ((struct rvar_bucket_t *)ret)->buckets[bidx] += rs->buckets[i];
        bucket += rs->bucket_size;
      }
    }

  }

  out = ret->percentile(ret, 0.999);
  ret->free(ret);
  return out;
}

#define EPS 1e-6
risk_cost_t
step_func_cost(struct risk_cost_func_t *t, rvar_type_t val) {
  struct risk_cost_func_step_t *r = (struct risk_cost_func_step_t *)t;
  risk_cost_t prev_value = r->pairs[r->nsteps-1].cost;

  // Sample string: 0/50-99.5/30-99.9/10-100/0
  for (int i = r->nsteps - 1; i >= 0; --i){
    if (r->pairs[i].step + EPS < val ) {
      return prev_value;
    }
    prev_value = r->pairs[i].cost;
  }

  return r->pairs[0].cost;
}

int _rcf_cmp(
    void const *v1, void const *v2) {
  struct _rcf_pair_t *p1 = (struct _rcf_pair_t *)v1;
  struct _rcf_pair_t *p2 = (struct _rcf_pair_t *)v2;

  if (p1->step < p2->step)      return -1;
  else if (p1->step > p2->step) return  1;
  else                          return  0;
}

struct risk_cost_func_t *
risk_cost_stepped_from_string(char const *string) {
  struct risk_cost_func_step_t *ret = malloc(sizeof(struct risk_cost_func_step_t));
  ret->cost = step_func_cost;

  char *dup = strdup(string);
  char *ptr = dup;
  int nsteps = 1;
  while (*ptr != 0) {
    if (*ptr == '-')
      nsteps++;
    ptr++;
  }
  ret->nsteps = nsteps;
  ret->pairs = malloc(sizeof(struct _rcf_pair_t) * nsteps);

  ptr = strtok(dup, "-");
  nsteps = 0;
  while (ptr != 0) {
    rvar_type_t step;
    risk_cost_t cost;
    sscanf(ptr, "%lf/%lf", &step, &cost); 
    ret->pairs[nsteps].step = (100 - step)/100.0;
    ret->pairs[nsteps].cost = cost;
    ptr = strtok(0, "-");
    nsteps += 1;
  }

  qsort(ret->pairs, nsteps, sizeof(struct _rcf_pair_t), _rcf_cmp);

  ret->rvar_to_cost = _default_rvar_to_cost;
  ret->cost = step_func_cost;

  info("Created a step function for the risk.");
  free(dup);
  return (struct risk_cost_func_t *)ret;
}

struct risk_cost_func_t *
risk_cost_string_to_func(char const *value) {
  char func_name[64] = {0};
  char rest[1024]    = {0};

  sscanf(value, "%[^-]-%s", func_name, rest);
  struct risk_cost_func_t *ret = 0;

  if (strcmp(func_name,  "stepped") == 0) {
    ret = risk_cost_stepped_from_string(rest);
  } else {
    panic("Couldn't find the function: %s", value);
  }

  return ret;
}
