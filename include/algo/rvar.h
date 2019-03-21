#ifndef _ALGO_RANDVAR_H_          
#define _ALGO_RANDVAR_H_         

#include <stdint.h>
#include <stdlib.h>

typedef double rvar_type_t;

/*
 * Random variable datastructures
 *
 * There are two types of random variables:
 *
 * 1) Sampled: where the sampled data is kept in an array (lossless).
 * 2) Bucketed: where a summary of data is kept in a histogram (lossy).
 *
 * Most operations (e.g., convolutions) on the sampled data result in a bucketed
 * output to save memory space.
 *
 * TODO: Can probably extend to add a sparse random variable too, incase the
 * probability distribution is too skewed.
 *
 * TODO: Should add a warning when the bucket size is too small (or
 * automatically choose the bucket size somehow).
 */

enum RVAR_TYPE {
  SAMPLED, BUCKETED,
};

struct rvar_t {
  rvar_type_t     (*expected)  (struct rvar_t const*);
  rvar_type_t     (*percentile)(struct rvar_t const*, float);
  struct rvar_t * (*convolve)  (struct rvar_t const*, struct rvar_t const*, rvar_type_t bucket_size);
  struct rvar_bucket_t * (*to_bucket) (struct rvar_t const*, rvar_type_t);

  void (*free)(struct rvar_t *);
  char * (*serialize)(struct rvar_t *, size_t *size);
  void (*plot)(struct rvar_t const *);
  struct rvar_t * (*copy)(struct rvar_t const *);

  enum RVAR_TYPE _type;
};

// Create a bucketized RVar value
struct rvar_t *rvar_bucket_create(rvar_type_t, rvar_type_t, uint32_t);

// Create a sampled RVar value
struct rvar_t *rvar_sample_create(unsigned);

// Rvar sampled type
struct rvar_sample_t {
  struct rvar_t;
  rvar_type_t low, high;        
  uint32_t num_samples;      
  rvar_type_t *vals;            
};

// Rvar Bucketized type
struct rvar_bucket_t {
  struct rvar_t;
  rvar_type_t bucket_size;
  rvar_type_t low;

  uint32_t    nbuckets;
  rvar_type_t *buckets;
};


struct rvar_t *rvar_deserialize(char const *data);
void rvar_sample_finalize(struct rvar_sample_t *rvar, uint32_t steps);
struct rvar_t *rvar_sample_create_with_vals(rvar_type_t *vals, uint32_t nvals);
struct rvar_t *rvar_zero(void);

/* Combines a bunch of rvars with corresponding distributions.
 *
 * Basically take each rvar, scale it with its associated dist, add it to a
 * "mega" rvar.  The final rvar is a combination of multiple "scenarios" that
 * could happen and have been simulated independently.
 */
struct rvar_t *rvar_compose_with_distributions(
    struct rvar_t **rvars,
    double *dists,
    unsigned len);

//rvar_bucket_t *rvar_to_bucket(struct

#endif // _ALGO_RANDVAR_H_   
