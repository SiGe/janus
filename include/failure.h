#ifndef _FAILURE_H_
#define _FAILURE_H_

#include "algo/rvar.h"

struct plan_iterator_t;
struct exec_t;
struct network_t;


/* TODO: Should guarantee that failure_model_t can be used concurrently from
 * multiple threads.  In reality, this should not be a problem since most
 * likely, the failure model is just a simple function that computes the
 * failure under some scenario.  However, we may want to ensure it, somehow.
 *
 * For now, the failure model itself is passed as a const to ensure that it is
 * not modified by the apply function.
 *
 * The rest of the items that are passed are created independently per thread,
 * so it should be fine.
 */
struct failure_model_t {
  /* Apply the failure model to a subplan---
   *
   * Each failure model applies the failure in its own way */
  struct rvar_t * (*apply)(
    struct failure_model_t const *,
    struct network_t *,
    struct plan_iterator_t *,  // Use the plan iterator to get the least
                               // dominative subplan_id
    struct rvar_t **,          // List of packet-loss variables for subplan XX
    unsigned subplan_id
  );
};

/* Combine multiple failure models
 *
 * Should be used with care cause in reality the failures could depend on each
 * other and combining them carelessly results in wrong probabilities
 */
struct failure_model_composite_t {
  struct failure_model_t;


  /* List of models in this composite failure structure */
  struct array_t *models;

};

/* Indepenent switch failure model
 *
 * The general theme of how this works is a bit complex and depends on the
 * topology ATM---it is possible to decouple it from the topology by using
 * Nauty, however it takes time, which I don't have at the moment.
 *
 * Back to the explanation:
 *
 * So we calculate a random variable for each scenario of concurrent switch
 * failures from [0 .. max_concurrent_switch_failure] switches. Then we "sum"
 * these random variables using weights.  Concretely:
 *
 * \sum_{over i = 0 .. mcsf} Pr_i * R_i
 *
 * The rest of this explains how we compute Pr_i and R_i:
 *
 * Computing Pr_i is basically the probability of a "i" switch failure while we
 * are running a subplan.  To complicate it even further, the probability AND
 * the random variable depends on where the switch failure happens.  Having 2
 * switch failures in a pod where 0 switches are getting upgraded is very
 * different than running 2 switch failures in a pod where all except 2 switches
 * are getting upgraded.  This becomes even more complex if you consider
 * topological complexities (thank lord we aren't touching nauty ATM). So
 * basically, we need to break down each computation further to cases with
 * "isomorphic networks"
 *
 * Jupiter---
 *
 * In Jupiter, isomorphic networks are rather easy to identify.  Topology is
 * super symmetric so all we need to do is to "count" how many switches are
 * getting upgraded in each pod and the core.  That is, it's a tuple of size
 * N = #pods + 1 (core switch count).  Failures can be represented as such:
 *
 * (F1, ...., FN)
 *
 * And so the number of switches that get upgraded is:
 *
 * (F1+S1, ..., FN+SN)
 *
 * The risk associated with this plan is equal to the risk of upgrading it.
 * However, calculating the exact probability is costly so we go with the least
 * "dominative" subplan in the list of subplans we already have:
 *
 * A least dominative subplan is one where:
 *
 * (F1+S1=<P1, ..., FN+SN=<PN)
 *
 * This guarantees that R_least_dominative_subplan >= R_i
 *
 * Now we have the risk value, R_i.  We still have to calculate the probability
 * for this risk value happening.  Without going into too much details, to
 * calculate P_i, we can use---it's pretty straightforward:
 *
 * Choose(F1, Remaining switches in F1) * ... *\
 * Choose(FN, Remaining switches in FN) * ... *\
 * (switch_failure_rate) ** (\sum(F1...FN))
 *
 */
struct failure_model_independent_t {
  struct failure_model_t;

  /* Maximum number of concurrent switch failures
   *
   * In general, we shouldn't need to supply this and the program should be able
   * to identify the optimal maximum number of concurrent switches.
   * */
  unsigned max_concurrent_switch_failure;

  /* The probability of a single switch failure.
   *
   * We assume that switches fail independently from each other.  For concurrent
   * dependent switch failure, we need a different model of operation.
   */
  double  switch_failure_probability;
};

#endif
