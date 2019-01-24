#ifndef _RANDVAR_H_          
#define _RANDVAR_H_         

#include <stdint.h>

typedef double rvar_type_t;
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
  char * (*serialize)(struct rvar_t *, int *size);

  enum RVAR_TYPE _type;
};


// Create a bucketized RVar value
struct rvar_t *rvar_bucket_create(rvar_type_t, rvar_type_t, uint32_t);

// Create a sampled RVar value
struct rvar_t *rvar_sample_create(int);

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


// Monte carlo methods for keeping single or multiple RVs
struct rvar_sample_t *rvar_monte_carlo(
    monte_carlo_run_t run,
    int nsteps,
    void *data);

struct rvar_sample_t *rvar_monte_carlo_multi(
    monte_carlo_run_multi_t run,
    int nsteps, int nvars,
    void *data);

struct rvar_sample_t *rvar_monte_carlo_parallel(
    monte_carlo_run_t run, // Monte carlo runner
    void *data,            // Data to pass to each instance of monte-carlo run (this should be an array of size nsteps)
    int nsteps,            // Amount of data
    int size,              // Size of each data segment
    int num_threads        // Number of thread to use or 0 for automatic calculation
);

struct rvar_t *rvar_deserialize(char const *data);

//rvar_bucket_t *rvar_to_bucket(struct

#endif // _RANDVAR_H_   
