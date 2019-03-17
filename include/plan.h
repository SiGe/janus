#ifndef _PLAN_H_
#define _PLAN_H_

#include <stdint.h>

#include "khash.h"

struct network_t;
typedef uint32_t mop_steps_t;


enum BLOCK_TYPE {
  BT_CORE, BT_POD_AGG, BT_POD_TOR
};

/* Block identifier in a mop
 *
 * A block is either the core switches, the aggregate switches, or the tor
 * switches in a pod.  Frankly, it is not a good enough abstraction for more
 * complex topology so it is only of value for Jupiter like topology.
 *
 * TODO: Maybe there is a better abstraction here?  Thankfully, this works for
 * FatTree ... so it may work for others.  For example,  in case of FatTree we
 * should break down the aggregate switches into smaller blocks.  Each
 * aggregate should have its own block.  Similarly, each k-identical core
 * switches should be in the same block.
 *
 * - Omid 3/16/2019
 * */
struct mop_block_id_t {
  enum BLOCK_TYPE type;
  int  id;
};

/* Returns the block statistics */
struct mop_block_stats_t {
  struct mop_block_id_t id;
  int all_switches;
  int down_switches;
};

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

  /* Size of the mop */
  int         (*size)           (struct mop_t *);

  /* Explain the mop in the context of the network */
  char *      (*explain)        (struct mop_t *, struct network_t *);

  /* Return the block statistics of a mop */
  int (*block_stats) (struct mop_t *, struct network_t *, struct mop_block_stats_t **);
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

  /* Explain a specific mop in text format */
  char * (*explain)(struct plan_iterator_t *, int id);

  /* This returns a preference score for a subplan */
  double (*pref_score)(struct plan_iterator_t *, int id);

  /* Return a list of subplans for the current id */
  void (*plan)(struct plan_iterator_t *, int **ret, int *size);

  /* Returns the maximum number of subplans */
  int (*subplan_count)(struct plan_iterator_t *);

  /* Returns the ID of the first subplan that takes down more capacity
   * (strictly and spatially) more than the passed block state */
  int (*least_dominative_subplan)(struct plan_iterator_t *, struct mop_block_stats_t *blocks, int nblocks);
};

#endif // _PLAN_H_
