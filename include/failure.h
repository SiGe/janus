#ifndef _FAILURE_H_
#define _FAILURE_H_

struct failure_model_t {
};

struct subplans_t {
};

/* Apply the failure model to subplan data */
int failure_model_apply(
    struct failure_model_t *,
    struct subplans_t *);


#endif
