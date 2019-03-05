#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "gnuplot_i/gnuplot_i.h"
#include "thpool/thpool.h"
#include "util/common.h"
#include "util/log.h"

#include "algo/rvar.h"

#define BUCKET_N(r) (r->nbuckets);
#define BUCKET_H(r) (r->low + r->bucket_size * r->nbuckets);
#define HEADER_SIZE (sizeof(enum RVAR_TYPE))

#define RVAR_PLOT_PATH "/tmp/planner.rvar.XXXXXX"


static char *_rvar_header(enum RVAR_TYPE type, char *buffer) {
  *(enum RVAR_TYPE*)buffer = type;
  return (buffer + HEADER_SIZE);
}

static char *_sample_serialize(struct rvar_t *rvar, int *size) {
  struct rvar_sample_t *rv = (struct rvar_sample_t*)rvar;
  *size = HEADER_SIZE + sizeof(rv->num_samples) + rv->num_samples * sizeof(rvar_type_t);
  char *buffer = malloc(*size);
  char *ptr = _rvar_header(rv->_type, buffer);

  // save num_samples
  *(uint32_t *)ptr = rv->num_samples;
  ptr += sizeof(uint32_t);

  for (uint32_t i = 0; i < rv->num_samples; ++i) {
    // save data
    *(rvar_type_t *)ptr = rv->vals[i];
    ptr += sizeof(rvar_type_t);
  }

  return buffer;
}

static struct rvar_t *_sample_copy(struct rvar_t const *rvar) {
  struct rvar_sample_t *rv = (struct rvar_sample_t *)rvar;
  size_t size = sizeof(rvar_type_t) * rv->num_samples;
  rvar_type_t *vals = malloc(size);
  memcpy(vals, rv->vals, size);
  return rvar_sample_create_with_vals(vals, rv->num_samples);
}


static struct rvar_t *_bucket_copy(struct rvar_t const *rvar) {
  struct rvar_bucket_t *rv = (struct rvar_bucket_t *)rvar;
// Create a bucketized RVar value
  struct rvar_bucket_t *ret = (struct rvar_bucket_t *)rvar_bucket_create(rv->low, rv->bucket_size, rv->nbuckets);
  size_t size = sizeof(rvar_type_t) * rv->nbuckets;
  memcpy(ret->buckets, rv->buckets, size);
  return (struct rvar_t *)ret;
}

static char *_bucket_serialize(struct rvar_t *rvar, int *size) {
  struct rvar_bucket_t *rv = (struct rvar_bucket_t*)rvar;
  *size = HEADER_SIZE + sizeof(rvar_type_t) /* bucket_size */
    + sizeof(uint32_t) /* nbuckets */
    + sizeof(rvar_type_t) /* low */
    + rv->nbuckets * sizeof(rvar_type_t);

  char *buffer = malloc(*size);
  char *ptr = _rvar_header(rv->_type, buffer);

  // save bucket_size
  *(rvar_type_t*)ptr = rv->bucket_size;
  ptr += sizeof(rvar_type_t);

  // save nbuckets
  *(uint32_t*)ptr = rv->nbuckets;
  ptr += sizeof(uint32_t);

  // save nbuckets
  *(rvar_type_t*)ptr = rv->low;
  ptr += sizeof(rvar_type_t);


  for (uint32_t i = 0; i < rv->nbuckets ; ++i) {
    // save data
    *(rvar_type_t *)ptr = rv->buckets[i];
    ptr += sizeof(rvar_type_t);
  }

  return buffer;
}

static void
_setup_gnuplot(gnuplot_ctrl *h1) {
  gnuplot_cmd(h1, "set terminal dumb");
  gnuplot_cmd(h1, "set nokey");
}

static void 
_sample_plot(struct rvar_t const *rs) {
  struct rvar_sample_t const *sample = (struct rvar_sample_t const*)(rs);
  char buffer[] = RVAR_PLOT_PATH;
  int fd = mkstemp(buffer);
  if (fd == -1)
    panic("Couldn't create the file for plotting :(");

  char line[1024] = {0};
  rvar_type_t index = 0;
  rvar_type_t prev = sample->vals[0];

  write(fd, "0\t0\n", 4);
  for (int i = 0; i < sample->num_samples; ++i) {
    if (sample->vals[i] == prev) {
      index++;
      continue;
    }

    snprintf(line, 1024, "%lf\t%lf\n", prev, index / sample->num_samples);
    write(fd, line, strlen(line));

    index = 0;
    prev = sample->vals[i];
    continue;
  }

  if (index != 0) {
    snprintf(line, 1024, "%lf\t%lf\n", prev, index / sample->num_samples);
    write(fd, line, strlen(line));
  }

  fsync(fd);
  gnuplot_ctrl *h1 = gnuplot_init();
  _setup_gnuplot(h1);
  snprintf(line, 1024, "plot \"%s\" using 1:2 with boxes", buffer);
  gnuplot_cmd(h1, line);
  gnuplot_close(h1);
  close(fd);
}


static rvar_type_t
_sample_percentile(struct rvar_t const *rs, float percentile) {
    struct rvar_sample_t *r = (struct rvar_sample_t *)rs;
    float fidx = percentile * (r->num_samples - 1);
    float hidx = ceil(fidx);
    float lidx = floor(fidx);

    rvar_type_t hval = r->vals[(int)hidx];
    rvar_type_t lval = r->vals[(int)lidx];
    if (hidx == lidx)
      return hval;

    return ((hval * (hidx - fidx) + lval * (fidx - lidx)));
}

static rvar_type_t
_sample_expected(struct rvar_t const *rs) {
    struct rvar_sample_t *r = (struct rvar_sample_t *)rs;
    rvar_type_t ret = 0;
    rvar_type_t *val = r->vals;
    for (int i = 0 ; i < r->num_samples; ++i) {
        ret += *val;
        val++;
    }

    return ret/r->num_samples;
}

static struct rvar_bucket_t *
_sample_to_bucket(struct rvar_t const *rs, rvar_type_t bucket_size) {
    struct rvar_sample_t *r = (struct rvar_sample_t *)rs;

    int num_buckets = (int)(ceil((r->high - r->low)/bucket_size)) + 1;
    struct rvar_t *ret = rvar_bucket_create(r->low, bucket_size, num_buckets);
    struct rvar_bucket_t *rb = (struct rvar_bucket_t *)ret;

    rvar_type_t *val = r->vals;
    rvar_type_t low = r->low;

    for (int i = 0; i < r->num_samples; ++i) {
      rb->buckets[(int)((*val - low)/bucket_size)]++;
      val++;
    }

    for (int i = 0; i < num_buckets; ++i) {
        rb->buckets[i] /= r->num_samples;
    }

    return rb;
}


static
void _sample_free(struct rvar_t *rs) {
    struct rvar_sample_t *rvar = (struct rvar_sample_t *)rs;
    if (!rvar) return;

    free(rvar->vals);
    free(rvar);
}

static
struct rvar_t *_sample_convolve(struct rvar_t const *left, struct rvar_t const *right, rvar_type_t bucket_size) {
    // we know that left is always SAMPLED
    struct rvar_bucket_t *rr = (struct rvar_bucket_t *)right;
    if (right->_type == SAMPLED)
        rr = right->to_bucket(right, bucket_size);
    struct rvar_bucket_t *ll = left->to_bucket(left, bucket_size);
    struct rvar_bucket_t *output = (struct rvar_bucket_t *)rvar_bucket_create(
            ll->low + rr->low, bucket_size, (ll->nbuckets + rr->nbuckets - 1));

    // We are gonna have an rvar_bucket_t of size ll->nbuckets + rr->nbuckets - 1
    for (uint32_t i = 0; i < ll->nbuckets; ++i) {
        double ll_pdf = ll->buckets[i];
        for (uint32_t j = 0; j < rr->nbuckets; ++j) {
            //TODO: Error can accumulate here ... need to do it another way,
            //e.g., keep integers and round up/down at some point
            output->buckets[i + j] += ll_pdf * rr->buckets[j];
        }
    }
    ll->free((struct rvar_t *)ll);
    return (struct rvar_t *)output;
}

static
void rvar_sample_init(struct rvar_sample_t *ret) {
    ret->expected = _sample_expected;
    ret->percentile = _sample_percentile;
    ret->free = _sample_free;
    ret->convolve = _sample_convolve;
    ret->to_bucket = _sample_to_bucket;
    ret->serialize = _sample_serialize;
    ret->plot = _sample_plot;
    ret->copy = _sample_copy;

    ret->_type = SAMPLED;
}

struct rvar_t *rvar_sample_create(int nsamples) {
    struct rvar_sample_t *ret = malloc(sizeof(struct rvar_sample_t));
    memset(ret, 0, sizeof(struct rvar_sample_t));
    ret->vals = malloc(sizeof(rvar_type_t) * nsamples);
    ret->num_samples = nsamples;
    rvar_sample_init(ret);
    
    return (struct rvar_t*)ret;
}

struct rvar_t *rvar_sample_create_with_vals(
    rvar_type_t *vals, uint32_t nsize) {
    struct rvar_sample_t *ret = malloc(sizeof(struct rvar_sample_t));
    memset(ret, 0, sizeof(struct rvar_sample_t));
    ret->vals = vals;
    ret->num_samples = nsize;
    rvar_sample_init(ret);
    rvar_sample_finalize(ret, nsize);
    
    return (struct rvar_t*)ret;
}

static rvar_type_t
_bucket_percentile(struct rvar_t const *rs, float percentile) {
    struct rvar_bucket_t *r = (struct rvar_bucket_t *)rs;
    rvar_type_t cdf = 0;

    for (uint32_t i = 0; i < r->nbuckets; ++i) {
        rvar_type_t pdf = r->buckets[i];
        if (cdf + pdf > percentile) {
            // TODO: can scale the return value to consider what portion of
            // percentile comes from i and what portion comes from i+1
            return (r->low + r->bucket_size * i + ((percentile - cdf)) / pdf * r->bucket_size);
        }
        cdf += pdf;
    }
    return r->low + r->bucket_size * r->nbuckets;
}

static rvar_type_t
_bucket_expected(struct rvar_t const *rs) {
    struct rvar_bucket_t *r = (struct rvar_bucket_t *)rs;
    rvar_type_t exp = 0;
    rvar_type_t delta = r->low;

    for (uint32_t i = 0; i < r->nbuckets; ++i) {
        exp += r->buckets[i] * delta;
        delta += r->bucket_size;
    }

    return exp;
}

static void _bucket_plot(struct rvar_t const *rs) {
  struct rvar_bucket_t const *buck = (struct rvar_bucket_t const*)(rs);
  char buffer[] = RVAR_PLOT_PATH;
  int fd = mkstemp(buffer);
  if (fd == -1)
    panic("Couldn't create the file for plotting :(");

  char line[1024] = {0};
  rvar_type_t x = buck->low;
  for (int i = 0; i < buck->nbuckets; ++i) {
    snprintf(line, 1024, "%lf\t%lf\n", (double)buck->buckets[i]/(double)buck->nbuckets, x);
    write(fd, line, strlen(line));
    x += buck->bucket_size;
  }
  fsync(fd);

  gnuplot_ctrl *h1 = gnuplot_init();
  _setup_gnuplot(h1);

  snprintf(line, 1024, "plot \"%s\" using 1:2 with boxes", buffer);
  gnuplot_cmd(h1, line);
  gnuplot_close(h1);
  close(fd);
}

static void _bucket_free(struct rvar_t *rs) {
    struct rvar_bucket_t *r = (struct rvar_bucket_t *)rs;
    if (!r) return;

    free(r->buckets);
    free(r);
}

static struct rvar_t *_bucket_convolve(struct rvar_t const *left, struct rvar_t const *right, rvar_type_t bucket_size) {
    // we know that left is always BUCKETED
    struct rvar_bucket_t const *rr = (struct rvar_bucket_t *)right;
    if (right->_type == SAMPLED)
        rr = right->to_bucket(right, bucket_size);
    struct rvar_bucket_t const *ll = (struct rvar_bucket_t *)left;
    struct rvar_bucket_t *output = (struct rvar_bucket_t *)rvar_bucket_create(
            ll->low + rr->low, bucket_size, (ll->nbuckets + rr->nbuckets - 1));

    // We are gonna have an rvar_bucket_t of size ll->nbuckets + rr->nbuckets - 1
    for (uint32_t i = 0; i < ll->nbuckets; ++i) {
        double ll_pdf = ll->buckets[i];
        for (uint32_t j = 0; j < rr->nbuckets; ++j) {
            //TODO: Error can accumulate here ... need to do it another way,
            //e.g., keep integers and round up/down at some point
            output->buckets[i + j] += ll_pdf * rr->buckets[j];
        }
    }
    return (struct rvar_t *)output;
}

static struct rvar_t *rvar_bucket_create(rvar_type_t low, rvar_type_t bucket_size, uint32_t nbuckets) {
    struct rvar_bucket_t *output = malloc(sizeof(struct rvar_bucket_t));
    output->buckets = malloc(sizeof(rvar_type_t) * nbuckets);
    memset(output->buckets, 0, sizeof(rvar_type_t) * nbuckets);
    output->low = low;
    output->bucket_size = bucket_size;
    output->nbuckets = nbuckets;

    output->_type = BUCKETED;
    output->expected = _bucket_expected;
    output->percentile = _bucket_percentile;
    output->convolve = _bucket_convolve;
    output->serialize = _bucket_serialize;
    output->free = _bucket_free;
    output->plot = _bucket_plot;
    output->copy = _bucket_copy;

    return (struct rvar_t *)output;
}

static int
_float_comp(const void *v1, const void *v2) {
    rvar_type_t f1 = *(rvar_type_t*)(v1);
    rvar_type_t f2 = *(rvar_type_t*)(v2);

    if      (f1 < f2) return -1;
    else if (f1 > f2) return  1;
    else              return  0;
}

/* TODO: This is so stupid, we shouldn't need to do this sorting ... it
 * should be done automatically, sort of.  Or be done lazily, if data gets
 * added to rvar and it isn't "optimized."
 * - Omid 1/22/2019
 * */
void rvar_sample_finalize(struct rvar_sample_t *rv, uint32_t nsteps) {
    /* TODO: This is so stupid, we shouldn't need to do this */
    rv->num_samples = nsteps;
    qsort(rv->vals, nsteps, sizeof(rvar_type_t), _float_comp);
    rv->low = rv->vals[0];
    rv->high = rv->vals[nsteps-1];
}

struct rvar_sample_t *rvar_monte_carlo(
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

struct rvar_sample_t *rvar_monte_carlo_multi(
        monte_carlo_run_multi_t run,
        int nsteps, int nvars, void *data) {
    struct rvar_sample_t *vars = malloc(sizeof(struct rvar_sample_t) * nvars);
    rvar_type_t **ptrs = malloc(sizeof(rvar_type_t *) * nvars);

    for (int i = 0; i < nvars; ++i) {
        vars[i].vals = malloc(sizeof(rvar_type_t) * nsteps);
        rvar_sample_init(&vars[i]);

        ptrs[i] = vars[i].vals;
    }


    for (int i = 0; i < nsteps; ++i) {
        run(data, ptrs, nvars);

        for (int j = 0; j < nvars; ++j)
            ptrs[j]++;
    }

    /* TODO: This is so stupid, we shouldn't need to do this */
    for (int i = 0; i < nvars; ++i) {
      rvar_sample_finalize(&vars[i], nsteps);
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

rvar_type_t *rvar_monte_carlo_parallel_ordered(
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


struct rvar_sample_t *rvar_monte_carlo_parallel(
    monte_carlo_run_t run,
    void *data,
    int nsteps,
    int dsize,
    int num_threads) {
  rvar_type_t *vals = rvar_monte_carlo_parallel_ordered(run, data, nsteps, dsize, num_threads);
  struct rvar_sample_t *rv = (struct rvar_sample_t *)rvar_sample_create_with_vals(vals, nsteps);
  rvar_sample_finalize(rv, nsteps);

  return rv;
}

struct rvar_t *_rvar_deserialize_sample(char const *data) {
  char const *ptr = data;

  uint32_t nsamples = *(uint32_t *)ptr;
  ptr += sizeof(uint32_t);

  struct rvar_sample_t *rv = (struct rvar_sample_t *)rvar_sample_create(nsamples);
  for (uint32_t i = 0; i < nsamples; ++i) {
    rv->vals[i] = *(rvar_type_t*)ptr;
    ptr += sizeof(rvar_type_t);
  }

  rv->num_samples = nsamples;
  rvar_sample_finalize(rv, nsamples);

  return (struct rvar_t *)rv;
}

struct rvar_t *_rvar_deserialize_bucket(char const *data) {
  char const *ptr = data;

  rvar_type_t bucket_size = *(rvar_type_t*)ptr;
  ptr += sizeof(rvar_type_t);

  uint32_t nbuckets = *(uint32_t*)ptr;
  ptr += sizeof(uint32_t);

  rvar_type_t low = *(rvar_type_t*)ptr;
  ptr += sizeof(rvar_type_t);

  struct rvar_bucket_t *rv = (struct rvar_bucket_t *)rvar_bucket_create(low, bucket_size, nbuckets);

  for (uint32_t i = 0; i < nbuckets ; ++i) {
    // load data
    rv->buckets[i] = *(rvar_type_t*)ptr;
    ptr += sizeof(rvar_type_t);
  }

  return (struct rvar_t *)rv;
}

struct rvar_t *rvar_deserialize(char const *data) {
  char const *ptr = data;

  enum RVAR_TYPE type = *(enum RVAR_TYPE *)ptr;
  ptr += HEADER_SIZE;

  if (type == SAMPLED) {
    return _rvar_deserialize_sample(ptr);
  } else if (type == BUCKETED) {
    return _rvar_deserialize_bucket(ptr);
  }

  panic("Unknown rvar_type_t: %d", type);
  return 0;
}

struct rvar_t *rvar_zero(void) {
  rvar_type_t *vals = malloc(sizeof(rvar_type_t));
  *vals = 0;
  return rvar_sample_create_with_vals(vals, 1);
}
