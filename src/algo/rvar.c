#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "util/log.h"

#include "algo/rvar.h"

#define BUCKET_N(r) (r->nbuckets);
#define BUCKET_H(r) (r->low + r->bucket_size * r->nbuckets);

static rvar_type_t
_sample_percentile(struct rvar_t const *rs, float percentile) {
    struct rvar_sample_t *r = (struct rvar_sample_t *)rs;
    float fidx = percentile * r->num_samples;
    float hidx = ceil(fidx);
    float lidx = floor(fidx);

    rvar_type_t hval = r->vals[(int)hidx];
    rvar_type_t lval = r->vals[(int)lidx];

    return ((hval * (hidx - fidx) + lval * (fidx - lidx)));
}

static rvar_type_t
_sample_expected(struct rvar_t const *rs) {
    struct rvar_sample_t *r = (struct rvar_sample_t *)rs;
    rvar_type_t ret = 0;
    rvar_type_t *val = 0;
    for (int i = 0 ; i < r->num_samples; ++i) {
        ret += *val++;
    }

    return ret/r->num_samples;
}

static struct rvar_bucket_t *
_sample_to_bucket(struct rvar_t const *rs, rvar_type_t bucket_size) {
    struct rvar_sample_t *r = (struct rvar_sample_t *)rs;

    int num_buckets = (int)(ceil((r->high - r->low)/bucket_size)) + 1;
    struct rvar_t *ret = rvar_bucket_create(r->low, bucket_size, num_buckets);
    struct rvar_bucket_t *rb = (struct rvar_bucket_t *)ret;

#define TO_BUCKET(bs, l, v) ((int)(floor((v - l)/bs)))

    for (int i = 0; i < r->num_samples; ++i) {
        rb->buckets[TO_BUCKET(bucket_size, r->low, r->vals[i])]++;
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
    return (struct rvar_t *)output;
}

static
void rvar_sample_init(struct rvar_sample_t *ret) {
    ret->expected = _sample_expected;
    ret->percentile = _sample_percentile;
    ret->free = _sample_free;
    ret->convolve = _sample_convolve;
    ret->to_bucket = _sample_to_bucket;
    ret->_type = SAMPLED;
}

struct rvar_t *rvar_sample_create(int nsamples) {
    struct rvar_sample_t *ret = malloc(sizeof(struct rvar_sample_t));
    ret->vals = malloc(sizeof(rvar_type_t) * nsamples);
    ret->num_samples = nsamples;
    rvar_sample_init(ret);
    
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


void _bucket_free(struct rvar_t *rs) {
    struct rvar_bucket_t *r = (struct rvar_bucket_t *)rs;
    if (!r) return;

    free(r->buckets);
    free(r);
}

struct rvar_t *_bucket_convolve(struct rvar_t const *left, struct rvar_t const *right, rvar_type_t bucket_size) {
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
struct rvar_t *rvar_bucket_create(rvar_type_t low, rvar_type_t bucket_size, uint32_t nbuckets) {
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
    output->free = _bucket_free;

    return (struct rvar_t *)output;
}

static int
_float_comp(const void *v1, const void *v2) {
    float f1 = *(float*)(v1);
    float f2 = *(float*)(v2);

    if (f1 < f2)
        return -1;
    else if (f2 > f1)
        return 1;
    return 0;
}

struct rvar_sample_t *rvar_monte_carlo(
        rvar_type_t (*single_run)(void *data),
        int nsteps, void *data) {
    struct rvar_sample_t *ret = (struct rvar_sample_t *)rvar_sample_create(nsteps);

    for (int i = 0; i < nsteps; ++i) {
        ret->vals[i] = single_run(data);
    }
    ret->num_samples = nsteps;
    qsort(ret->vals, nsteps, sizeof(rvar_type_t), _float_comp);
    ret->low = ret->vals[0];
    ret->high = ret->vals[nsteps-1];

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

    for (int i = 0; i < nvars; ++i) {
        vars[i].num_samples = nsteps;
        qsort(vars[i].vals, nsteps, sizeof(rvar_type_t), _float_comp);
        vars[i].low = vars[i].vals[0];
        vars[i].high = vars[i].vals[nsteps-1];
    }

    free(ptrs);
    return vars;
}
