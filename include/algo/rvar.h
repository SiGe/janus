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
 * Most operations (e.g., convolutions) on the sampled data result in a
 * bucketed output to save memory space.
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
  /* Returns the expected value of the random variable */
  rvar_type_t     (*expected)  (struct rvar_t const*);

  /* Returns the percentile of the random variable */
  rvar_type_t     (*percentile)(struct rvar_t const*, float);

  /* Convolves (aka adds) this random variable to another one */
  /* TODO: Maybe change the interface to add?  But then this becomes confusing
   * later on, e.g., why we don't have subtract, etc.  Convolve is sort of
   * explicit in the sense that it also require "independence" */
  struct rvar_t * (*convolve)  (struct rvar_t const*, 
      struct rvar_t const*, rvar_type_t bucket_size);


  /* Change a random variable to a bucketed random variable, i.e., a histogram.
   * The passed rvar_type_t is the size of the bucket */
  struct rvar_bucket_t * (*to_bucket) (struct rvar_t const*, rvar_type_t);

  /* Free random variable resources */
  void (*free)(struct rvar_t *);

  /* Serialize the random variable */
  char * (*serialize)(struct rvar_t *, size_t *size);

  /* Plot this random variable in terminal.  Uses gnuplot_i library. */
  void (*plot)(struct rvar_t const *);

  /* Create a copy of this random variable. */
  struct rvar_t * (*copy)(struct rvar_t const *);

  /* Type of the random variable */
  enum RVAR_TYPE _type;
};

/* A random variable that uses "sampled" values for its calculations. */
struct rvar_sample_t {
  struct rvar_t;
  rvar_type_t low, high;
  uint32_t num_samples;
  rvar_type_t *vals;
};

struct bucket_t {
  rvar_type_t val;
  rvar_type_t prob;
};

/* A random variable that keeps a histogram of the data for its calculations */
struct rvar_bucket_t {
  struct rvar_t;            /* Bucketed rvar is a rvar_t */
  struct bucket_t *buckets; /* Histogram buckets: the buckets are all sorted */
  unsigned nbuckets;        /* Number of buckets */
  rvar_type_t bucket_size;  /* Size of each bucket */
};

/* Deserialize the string into a random variable */
struct rvar_t *rvar_deserialize(char const *data);
struct rvar_t *rvar_sample_create_with_vals(rvar_type_t *vals, uint32_t nvals);
struct rvar_t *rvar_zero(void);
struct rvar_t *rvar_fixed(rvar_type_t value);

/* Create a sampled random variable */
struct rvar_t *rvar_sample_create(unsigned);

/* Create an empty bucketized random variable---typically shouldn't be needed or used */
struct rvar_t *rvar_bucket_create(rvar_type_t);

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

/* Creates a new rvar from a list of buckets */
struct rvar_t *rvar_from_buckets(
    struct bucket_t *buckets,
    unsigned nbuckets,
    rvar_type_t bucket_size);

//rvar_bucket_t *rvar_to_bucket(struct

#endif // _ALGO_RANDVAR_H_   
