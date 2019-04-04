#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "algo/array.h"
#include "util/common.h"
#include "util/log.h"

#include "risk.h"

#define ROUND_AND_CLAMP(val, round, max) MIN((floor((val)/round)*round), max)

risk_cost_t _default_rvar_to_cost (struct risk_cost_func_t *f, struct rvar_t *rvar) {
  //return rvar->percentile(rvar, 0.10);
  //return rvar->percentile(rvar, 0.999);
  return rvar->expected(rvar);
}

struct rvar_t *_default_rvar_to_rvar(struct risk_cost_func_t *f, struct rvar_t *rvar, rvar_type_t bucket_size) {
  if (bucket_size == 0)
    bucket_size = 1;

  struct rvar_t *ret = 0;

  if (rvar->_type == SAMPLED) {
    /* If sampled ... do value to value translation */
    struct rvar_sample_t *rs = (struct rvar_sample_t *)rvar;
    struct array_t *arr = array_create(sizeof(rvar_type_t), rs->num_samples);

    for (uint32_t i = 0; i < rs->num_samples; ++i) {
      rvar_type_t val = f->cost(f, rs->vals[i]);
      array_append(arr, &val);
    }

    rvar_type_t *vals = 0;
    array_transfer_ownership(arr, (void**)&vals);
    array_free(arr);

    ret = rvar_sample_create_with_vals(vals, rs->num_samples);
  } else if (rvar->_type == BUCKETED){
    /* If bucket, create a new bucket variable where:
     * low = min(cost) and num_buckets = (max(cost) - min(cost))/100?? */
    struct rvar_bucket_t *rs = (struct rvar_bucket_t *)rvar;

#define ROUND_TO_BUCKET(val, bs) (floor((val) * (bs))/(bs))

    struct array_t *arr = array_create(sizeof(struct bucket_t), rs->nbuckets);
    struct bucket_t bucket;
    struct bucket_t *ptr = rs->buckets;
    for (uint32_t i = 0; i < rs->nbuckets; ++i) {
      bucket.val = ROUND_TO_BUCKET(f->cost(f, ptr->val), bucket_size);
      bucket.prob = ptr->prob;
      array_append(arr, &bucket);
      ptr++;
    }

    struct bucket_t *buckets = 0;
    array_transfer_ownership(arr, (void**)&buckets);
    array_free(arr);

    ret = rvar_from_buckets(buckets, rs->nbuckets, bucket_size);
  }

  return ret;
}

#define EPS 1e-6
risk_cost_t
step_func_cost(struct risk_cost_func_t *t, rvar_type_t val) {
  struct risk_cost_func_step_t *r = (struct risk_cost_func_step_t *)t;
  risk_cost_t prev_value = r->pairs[r->nsteps-1].cost;

  // Sample string: 0/50-99.5/30-99.9/10-100/0
  for (unsigned i = r->nsteps; i != 0; --i){
    if (r->pairs[i-1].step + EPS < val ) {
      return prev_value;
    }
    prev_value = r->pairs[i-1].cost;
  }

  return r->pairs[0].cost;
}

risk_cost_t
linear_func_cost(struct risk_cost_func_t *t, rvar_type_t val) {
  struct risk_cost_func_linear_t *r = (struct risk_cost_func_linear_t *)t;
  return ROUND_AND_CLAMP((val) * r->slope, r->round, r->max);
}

risk_cost_t
poly_func_cost(struct risk_cost_func_t *t, rvar_type_t val) {
  struct risk_cost_func_poly_t *r = (struct risk_cost_func_poly_t *)t;
  return ROUND_AND_CLAMP(pow(val, r->power) * r->ratio, r->round, r->max);
}

risk_cost_t
exponential_func_cost(struct risk_cost_func_t *t, rvar_type_t val) {
  struct risk_cost_func_exponential_t *r = (struct risk_cost_func_exponential_t *)t;
  return ROUND_AND_CLAMP((exp(val * r->power) - 1) * r->ratio, r->round, r->max);
}

risk_cost_t
logarithmic_func_cost(struct risk_cost_func_t *t, rvar_type_t val) {
  struct risk_cost_func_logarithmic_t *r = (struct risk_cost_func_logarithmic_t *)t;
  return ROUND_AND_CLAMP(log(val * r->power + 1) * r->ratio, r->round, r->max);
}

int _rcf_cmp(
    void const *v1, void const *v2) {
  struct _rcf_pair_t *p1 = (struct _rcf_pair_t *)v1;
  struct _rcf_pair_t *p2 = (struct _rcf_pair_t *)v2;

  if (p1->step < p2->step)      return -1;
  else if (p1->step > p2->step) return  1;
  else                          return  0;
}

static struct risk_cost_func_t *
risk_cost_linear_from_string(char const *string) {
  struct risk_cost_func_linear_t *ret = malloc(sizeof(struct risk_cost_func_linear_t));
  ret->cost = linear_func_cost;
  ret->rvar_to_rvar = _default_rvar_to_rvar;
  ret->rvar_to_cost = _default_rvar_to_cost;

  sscanf(string, "%lf-%lf-%lf", &ret->slope, &ret->round, &ret->max);
  info_txt("Creating a linear function for the risk.");
  return (struct risk_cost_func_t *)ret;
}

static struct risk_cost_func_t *
risk_cost_poly_from_string(char const *string) {
  struct risk_cost_func_poly_t *ret = malloc(sizeof(struct risk_cost_func_poly_t));
  ret->cost = poly_func_cost;
  ret->rvar_to_rvar = _default_rvar_to_rvar;
  ret->rvar_to_cost = _default_rvar_to_cost;

  info_txt(string);
  sscanf(string, "%lf-%lf-%lf-%lf", &ret->power, &ret->ratio, &ret->round, &ret->max);
  info("Creating a poly function for the risk: %lf x (X ^ %lf)", ret->ratio, ret->power);
  return (struct risk_cost_func_t *)ret;
}

static struct risk_cost_func_t *
risk_cost_exponential_from_string(char const *string) {
  struct risk_cost_func_exponential_t *ret = malloc(sizeof(struct risk_cost_func_exponential_t));
  ret->cost = exponential_func_cost;
  ret->rvar_to_rvar = _default_rvar_to_rvar;
  ret->rvar_to_cost = _default_rvar_to_cost;

  sscanf(string, "%lf-%lf-%lf-%lf", &ret->power, &ret->ratio, &ret->round, &ret->max);
  info_txt("Creating an exponential function for the risk.");
  return (struct risk_cost_func_t *)ret;
}

static struct risk_cost_func_t *
risk_cost_logarithmic_from_string(char const *string) {
  struct risk_cost_func_logarithmic_t *ret = malloc(sizeof(struct risk_cost_func_logarithmic_t));
  ret->cost = logarithmic_func_cost;
  ret->rvar_to_rvar = _default_rvar_to_rvar;
  ret->rvar_to_cost = _default_rvar_to_cost;

  sscanf(string, "%lf-%lf-%lf-%lf", &ret->power, &ret->ratio, &ret->round, &ret->max);
  info_txt("Creating a logarithmic function for the risk.");
  return (struct risk_cost_func_t *)ret;
}

static struct risk_cost_func_t *
risk_cost_stepped_from_string(char const *string) {
  struct risk_cost_func_step_t *ret = malloc(sizeof(struct risk_cost_func_step_t));
  ret->cost = step_func_cost;

  char *dup = strdup(string);
  char *ptr = dup;
  unsigned nsteps = 1;
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

  ret->rvar_to_rvar = _default_rvar_to_rvar;
  ret->rvar_to_cost = _default_rvar_to_cost;
  ret->cost = step_func_cost;

  info_txt("Creating a step function for the risk.");
  free(dup);
  return (struct risk_cost_func_t *)ret;
}

struct risk_cost_func_t *
risk_cost_string_to_func(char const *value) {
  char func_name[64] = {0};
  char rest[1024]    = {0};

  sscanf(value, "%[^-]-%s", func_name, rest);
  struct risk_cost_func_t *ret = 0;

  if        (strcmp(func_name,  "stepped") == 0) {
    ret = risk_cost_stepped_from_string(rest);
  } else if (strcmp(func_name, "linear") == 0) {
    ret = risk_cost_linear_from_string(rest);
  } else if (strcmp(func_name, "exponential") == 0) {
    ret = risk_cost_exponential_from_string(rest);
  } else if (strcmp(func_name, "poly") == 0) {
    ret = risk_cost_poly_from_string(rest);
  } else if (strcmp(func_name, "logarithmic") == 0) {
    ret = risk_cost_logarithmic_from_string(rest);
  } else {
    panic("Couldn't find the function: %s", value);
  }

  return ret;
}
