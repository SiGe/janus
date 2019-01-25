#ifndef _PLAN_H_
#define _PLAN_H_

#include <stdint.h>

#include "khash.h"

struct network_t;
typedef uint32_t mop_steps_t;

/* A management operation has three functions: pre, operation, post.
 *
 * The pre function prepares the network for the management operation.  
 * The operation function telling us the duration that network remain in pre.
 * The post function rejoins the elements back to the network.
 */
struct mop_t {
  /* Apply a pre mop on the network */
  int             (*pre)        (struct mop_t *, struct network_t*);

  /* Apply a post mop on the network */
  int             (*post)       (struct mop_t *, struct network_t*);

  /* Number of steps that we should run this operation */
  mop_steps_t (*operation)      (struct mop_t *, struct network_t*);

  /* Free the mop */
  void        (*free)           (struct mop_t *);
};

struct jupiter_switch_mop_t {
    struct mop_t;

    struct jupiter_located_switch_t **switches;
    uint32_t nswitches;
    uint32_t ncap;
};

/* A planner has one interface ... a plan iterator that lets us iterate
 * throught the feasible set of plans.  
 *
 * Possibly each network would want to specialize the iterator (or we can write
 * a generic iterator that uses nauty in the background and does the prunin
 * automatically but that takes too much time ... for this paper).
 */
struct plan_t {
  struct plan_iterator_t* (*iter) (struct plan_t *);
};

/* Iterator that returns an ordered set of mops for a specific step of a plan.
 *
 * What we (probably?) want to do is to just iterate through all the subsets and
 * for the second step return the PLAN - the subset so that the long-term
 * planner can find the best plan.
 *
 * The first step is not commutative but the long-term plan IS commutative.  So
 * the query to the long-term planner would be: 
 *
 *    Give me the best set of steps to finish a plan under XX budget.
 *
 * And the long term planner would do a query to its internal database to find
 * that plan.
 */
struct plan_iterator_t {
  void (*begin) (struct plan_iterator_t *);
  void (*free)  (struct plan_iterator_t *);
  int  (*next)  (struct plan_iterator_t *);
  int  (*end)   (struct plan_iterator_t *);

  /* Return the mop for a subplan with specific id */
  struct mop_t * (*mop_for)(struct plan_iterator_t *, int id);

  /* Return a list of subplans for the current id */
  void (*plan)(struct plan_iterator_t *, int **ret, int *size);

  /* Returns the maximum number of subplans */
  int (*subplan_count)(struct plan_iterator_t *);
};

#endif // _PLAN_H_
