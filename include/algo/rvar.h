#ifndef _RANDVAR_H_          
#define _RANDVAR_H_         

#include <stdint.h>

typedef float rvar_type_t;
typedef rvar_type_t (*monte_carlo_run_t)(void *);
typedef void (*monte_carlo_run_multi_t)(void *, rvar_type_t **, int);

enum RVAR_TYPE {
  SAMPLED, BUCKETED,
};

struct rvar_t {
  rvar_type_t     (*expected)  (struct rvar_t const*);
  rvar_type_t     (*percentile)(struct rvar_t const*, float);
  struct rvar_t * (*convolve)  (struct rvar_t const*, struct rvar_t const*, rvar_type_t bucket_size);
  struct rvar_bucket_t * (*to_bucket) (struct rvar_t const*, rvar_type_t);

  void (*free)(struct rvar_t *);

  enum RVAR_TYPE _type;
};


struct rvar_bucket_t {
  struct rvar_t;
  rvar_type_t bucket_size;
  rvar_type_t low;

  uint32_t    nbuckets;
  rvar_type_t *buckets;
};

struct rvar_t *rvar_bucket_create(rvar_type_t, rvar_type_t, uint32_t);

struct rvar_sample_t {
  struct rvar_t;
  rvar_type_t low, high;        
  uint32_t num_samples;      
  rvar_type_t *vals;            
};

struct rvar_t *rvar_sample_create(int);
struct rvar_sample_t *rvar_monte_carlo(
    monte_carlo_run_t run,
    int nsteps,
    void *data);

struct rvar_sample_t *rvar_monte_carlo_multi(
    monte_carlo_run_multi_t run,
    int nsteps, int nvars,
    void *data);

struct rvar_sample_t *rvar_convolution(
    struct rvar_sample_t *first,
    struct rvar_sample_t *second);

void rvar_sample_free(struct rvar_sample_t *rvar);

//rvar_bucket_t *rvar_to_bucket(struct

#endif // _RANDVAR_H_   
