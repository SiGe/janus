#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "algo/array.h"
#include "gnuplot_i/gnuplot_i.h"
#include "thpool/thpool.h"
#include "util/common.h"
#include "util/log.h"

#include "algo/rvar.h"

#define BUCKET_N(r) (r->nbuckets);
#define BUCKET_H(r) (r->low + r->bucket_size * r->nbuckets);
#define HEADER_SIZE (sizeof(enum RVAR_TYPE))
#define PROB_ERR 5e-2
#define ASSERT_DIST(p) assert( ((p) < 1 + PROB_ERR) && ((p) > 1 - PROB_ERR))
#define ROUND_TO_BUCKET(val, bs) (floor((val) * (bs))/(bs))
#define RVAR_PLOT_PATH "/tmp/planner.rvar.XXXXXX"

/* TODO: Maybe can use http://people.ece.umn.edu/users/parhi/SLIDES/chap8.pdf
 * to improve the convolution speed.  Right now, it's so so ... bad.
 *
 * The idea is that IFFT(FFT(Rv1) * FFT(Rv2)) is faster than Rv1 (convolve) Rv2.
 * Can use: http://fftw.org/download.html
 *
 * The reasoning being that convolution in the "time" domain is equivalent to
 * multiplication in the "frequency" domain.  Meaning that N^2 operation
 * becomes N in the frequency domain.  Going from the time domain to the
 * frequency domain itself is Nlog(N).
 *
 * So overall it's a win.
 *
 * - Omid 04/04/2019 */

/* TODO: Another idea is to revert back to bounded RVars.  By doing so, we can use
 * array index to navigate the Rvar as opposed to a sorted linked list.
 *
 * - Omid 04/05/2019 */

static char *_rvar_header(enum RVAR_TYPE type, char *buffer) {
  *(enum RVAR_TYPE*)buffer = type;
  return (buffer + HEADER_SIZE);
}

static char *_sample_serialize(struct rvar_t *rvar, size_t *size) {
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

  // Create the copy
  struct rvar_bucket_t *ret = (struct rvar_bucket_t *)rvar_bucket_create(rv->bucket_size);
  size_t size = sizeof(struct bucket_t) * rv->nbuckets;
  ret->buckets = malloc(size);
  ret->nbuckets = rv->nbuckets;
  memcpy(ret->buckets, rv->buckets, size);
  return (struct rvar_t *)ret;
}

static char *_bucket_serialize(struct rvar_t *rvar, size_t *size) {
  struct rvar_bucket_t *rv = (struct rvar_bucket_t*)rvar;
  *size = HEADER_SIZE + 
    + sizeof(uint32_t) /* nbuckets */
    + sizeof(rvar_type_t) /* bucket_size */
    + (rv->nbuckets * sizeof(struct bucket_t)) /* buckets */;

  char *buffer = malloc(*size);
  char *ptr = _rvar_header(rv->_type, buffer);

  // Save nbuckets
  *(uint32_t*)ptr = rv->nbuckets;
  ptr += sizeof(uint32_t);

  // Save bucket_size
  *(rvar_type_t*)ptr = rv->bucket_size;
  ptr += sizeof(rvar_type_t);

  // Copy the buckets
  memcpy(ptr, rv->buckets, sizeof(struct bucket_t) * rv->nbuckets);

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
    panic_txt("Couldn't create the file for plotting :(");

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

    // Maximum number of buckets required
    unsigned max_num_buckets = (unsigned)(ceil((r->high - r->low)/bucket_size)) + 1;
    struct array_t *buckets = array_create(sizeof(struct bucket_t), max_num_buckets);

    struct bucket_t bucket;
    bucket.prob = 0; bucket.val = ROUND_TO_BUCKET(r->low, bucket_size);
    unsigned num_samples = r->num_samples;

    rvar_type_t *val = r->vals;
    for (int i = 0; i < r->num_samples; ++i) {
      if (*val >= bucket.val + bucket_size) {
        bucket.prob /= (double)(num_samples);
        array_append(buckets, &bucket);
        bucket.prob = 1; bucket.val = ROUND_TO_BUCKET(*val, bucket_size);
      } else {
        bucket.prob += 1;
      }

      val++;
    }

    // Append the last bit to the bucket
    if (bucket.prob != 0) {
      bucket.prob /= (double)(num_samples);
      array_append(buckets, &bucket);
    }

    struct rvar_bucket_t *ret = (struct rvar_bucket_t *)rvar_bucket_create(bucket_size);
    ret->nbuckets = array_size(buckets);

    // Transfer ownership of buckets and return
    array_transfer_ownership(buckets, (void**)(&ret->buckets));
    array_free(buckets);

    // Resize the bucket size
    ret->buckets = realloc(ret->buckets, sizeof(struct bucket_t) * ret->nbuckets);
    return ret;
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
    struct rvar_t *ret = ll->convolve(
        (struct rvar_t const *)ll, 
        (struct rvar_t const *)rr, 
        bucket_size);
    ll->free((struct rvar_t *)ll);
    return ret;

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
 *
 * Ack got this removed in most places so at least we can make it static.
 * */
static void _rvar_sample_finalize(struct rvar_sample_t *rv, uint32_t nsteps) {
    /* TODO: This is so stupid, we shouldn't need to do this */
    rv->num_samples = nsteps;
    qsort(rv->vals, nsteps, sizeof(rvar_type_t), _float_comp);
    rv->low = rv->vals[0];
    rv->high = rv->vals[nsteps-1];
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

struct rvar_t *rvar_sample_create(unsigned nsamples) {
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
    _rvar_sample_finalize(ret, nsize);
    
    return (struct rvar_t*)ret;
}

static rvar_type_t
_bucket_percentile(struct rvar_t const *rs, float percentile) {
    struct rvar_bucket_t *r = (struct rvar_bucket_t *)rs;
    rvar_type_t cdf = 0;

    for (uint32_t i = 0; i < r->nbuckets; ++i) {
        struct bucket_t *bucket = &r->buckets[i];
        rvar_type_t pdf = bucket->prob;
        if (cdf + pdf > percentile) {
            // TODO: can scale the return value to consider what portion of
            // percentile comes from i and what portion comes from i+1
            // Interpolate between bucket->val and bucket->val + bucket_size
            rvar_type_t low = bucket->val;
            // info("Low is: %lf, %lf, %lf, %lf, %lf", low, (percentile - cdf)/pdf, cdf, percentile, pdf);
            return low + r->bucket_size * (percentile - cdf) / pdf;
        }
        cdf += pdf;
    }

    // Just return the last bucket's end as the result
    return r->bucket_size + r->buckets[r->nbuckets-1].val;
}

static rvar_type_t
_bucket_expected(struct rvar_t const *rs) {
    struct rvar_bucket_t *r = (struct rvar_bucket_t *)rs;
    rvar_type_t exp = 0;
    struct bucket_t *bucket = r->buckets;

    for (uint32_t i = 0; i < r->nbuckets; ++i) {
        exp += bucket->prob * bucket->val;
        bucket += 1;
    }

    return exp;
}

static void _bucket_plot(struct rvar_t const *rs) {
  struct rvar_bucket_t const *buck = (struct rvar_bucket_t const*)(rs);
  char buffer[] = RVAR_PLOT_PATH;
  int fd = mkstemp(buffer);
  if (fd == -1)
    panic_txt("Couldn't create the file for plotting :(");

  char line[1024] = {0};
  // TODO: Not sure what this is doing.  Have to fix the plotting later on.
  //
  // - Omid 03/29/2019
  rvar_type_t x = buck->buckets->val;
  for (int i = 0; i < buck->nbuckets; ++i) {
    snprintf(line, 1024, "%lf\t%lf\n", (double)buck->buckets[i].val/(double)buck->nbuckets, x);
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

    if (r->buckets)
      free(r->buckets);

    free(r);
}

static struct rvar_t *_bucket_convolve(struct rvar_t const *left, struct rvar_t const *right, rvar_type_t bucket_size) {
    // we know that left is always BUCKETED
    struct rvar_bucket_t const *rr = (struct rvar_bucket_t *)right;
    if (right->_type == SAMPLED)
        rr = right->to_bucket(right, bucket_size);
    struct rvar_bucket_t const *ll = (struct rvar_bucket_t *)left;

    struct array_t *arr = array_create(
        sizeof(struct bucket_t), 
        ll->nbuckets + 10 /* Deal with close to empty samples */
        );

    // We are gonna have an rvar_bucket_t of size ll->nbuckets + rr->nbuckets - 1
    struct bucket_t bucket;

    // keep a rolling prob
    rvar_type_t cdf = 0;
    for (unsigned i = 0; i < ll->nbuckets; ++i) {
      for (unsigned j = 0; j < rr->nbuckets; ++j) {
        bucket.val = ll->buckets[i].val + rr->buckets[j].val;
        bucket.prob = ll->buckets[i].prob * rr->buckets[j].prob;

        array_append(arr, &bucket);
        cdf += bucket.prob;
      }
    }

    struct bucket_t *buckets = 0;
    unsigned size = array_size(arr);
    array_transfer_ownership(arr, (void**)&buckets);
    array_free(arr);

    if (cdf < 1 - PROB_ERR || cdf > 1 + PROB_ERR) {
      rvar_type_t sum = 0;
      rvar_type_t ratio = 1/cdf;
      for (unsigned i = 0; i < size; i++) {
        buckets[i].prob *= ratio;
        sum += buckets[i].prob;
      }
      ASSERT_DIST(sum);
    }

    struct rvar_t *ret = rvar_from_buckets(buckets, size, bucket_size);
    free(buckets);
    return ret;
}

struct rvar_t *rvar_bucket_create(rvar_type_t bucket_size) {
    struct rvar_bucket_t *output = malloc(sizeof(struct rvar_bucket_t));
    memset(output, 0, sizeof(struct rvar_bucket_t));
    output->buckets = 0; //malloc(sizeof(rvar_type_t) * nbuckets);
    output->nbuckets = 0;
    output->bucket_size = bucket_size;

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
  _rvar_sample_finalize(rv, nsamples);

  return (struct rvar_t *)rv;
}

struct rvar_t *_rvar_deserialize_bucket(char const *data) {
  char const *ptr = data;

  uint32_t nbuckets = *(uint32_t*)ptr;
  ptr += sizeof(uint32_t);

  rvar_type_t bucket_size = *(rvar_type_t*)ptr;
  ptr += sizeof(rvar_type_t);

  struct rvar_bucket_t *rv = (struct rvar_bucket_t *)rvar_bucket_create(bucket_size);
  size_t size = sizeof(struct bucket_t) * nbuckets;
  rv->buckets = malloc(size);
  memcpy(rv->buckets, ptr, size);

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
  return rvar_fixed(0);
}

struct rvar_t *rvar_fixed(rvar_type_t value) {
  rvar_type_t *vals = malloc(sizeof(rvar_type_t));
  *vals = value;
  return rvar_sample_create_with_vals(vals, 1);
}

int _sort_buckets(void const *p1, void const *p2) {
  struct bucket_t *b1 = (struct bucket_t *)p1;
  struct bucket_t *b2 = (struct bucket_t *)p2;

  if (b1->val < b2->val)      { return -1; }
  else if (b1->val > b2->val) { return  1; }
  else { return 0; }
}

struct rvar_t *rvar_compose_with_distributions(
    struct rvar_t **rvars,
    double *dists,
    unsigned len) {
  if (len == 0 || dists == 0 || rvars == 0)
    panic_txt("Passing nulls to rvar_compose_with_distribution");

  double bucket_size = INFINITY;
  // Get the total number of buckets and create a large enough space to hold
  // them all
  unsigned nbuckets = 0;
  rvar_type_t scale_sum = 0;
  for (int i = 0; i < len; ++i) {
    struct rvar_bucket_t *rv = (struct rvar_bucket_t *)rvars[i];
    nbuckets += rv->nbuckets;
    bucket_size = MIN(bucket_size, rv->bucket_size);
    scale_sum += dists[i];
  }

  scale_sum = 1/scale_sum;

  if (nbuckets == 0) {
    panic_txt("Number of buckets in the composition is zero.");
  }

  struct bucket_t *buckets = malloc(sizeof(struct bucket_t) * nbuckets);
  struct bucket_t *ptr = buckets;
  for (int i = 0; i < len; ++i) {
    struct rvar_bucket_t *rv = (struct rvar_bucket_t *)rvars[i];
    rvar_type_t scale = dists[i] * scale_sum;
    for (int j = 0; j < rv->nbuckets; ++j) {
      ptr[j].val = rv->buckets[j].val;
      ptr[j].prob = rv->buckets[j].prob * scale;
    }
    ptr += rv->nbuckets;
  }

  return rvar_from_buckets(buckets, nbuckets, bucket_size);
}

struct rvar_t *rvar_from_buckets(
    struct bucket_t *buckets,
    unsigned nbuckets, rvar_type_t bucket_size) {
  qsort(buckets, nbuckets, sizeof(struct bucket_t), _sort_buckets);
  struct array_t *arr = array_create(sizeof(struct bucket_t), nbuckets);
  struct bucket_t bucket;
  bucket.val = buckets[0].val * buckets[0].prob;
  bucket.prob = buckets[0].prob;
  rvar_type_t acc = 0;

  for (int i = 1; i < nbuckets; ++i) {
    /* Merge buckets that overlap */
    if (buckets[i].val == bucket.val) {
      // Accumulate
      bucket.prob += buckets[i].prob;
      continue;
    }

    /* Compression of RVAR: If we haven't added up enough entries, mash up a
     * few entries together */
    if (bucket.prob <= PROB_ERR) {
      bucket.prob += buckets[i].prob;
      bucket.val += buckets[i].val * buckets[i].prob;
      continue;
    }

    acc += bucket.prob;
    bucket.val /= bucket.prob;
    bucket.val = ROUND_TO_BUCKET(bucket.val, bucket_size);
    array_append(arr, &bucket);
    bucket.val = buckets[i].val * buckets[i].prob;
    bucket.prob = buckets[i].prob;
  }
  acc += bucket.prob;
  ASSERT_DIST(acc);

  // append the last bit
  bucket.val /= bucket.prob;
  bucket.val = ROUND_TO_BUCKET(bucket.val, bucket_size);
  array_append(arr, &bucket);

  struct rvar_bucket_t *ret = (struct rvar_bucket_t *)rvar_bucket_create(bucket_size);
  ret->nbuckets = array_size(arr);
  array_transfer_ownership(arr, (void**)&ret->buckets);
  array_free(arr);
  
  return (struct rvar_t *)ret;
}

