#ifndef _ALGO_GROUP_GEN_H_
#define _ALGO_GROUP_GEN_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/*
 * A data structure to keep the state of a group iterator.  Accepts an integer
 * and returns all the possible sets of integers that add up to that numbers.
 *
 * E.g., for group_iter_t 4, the possible ways to get to 4 are:
 * 1, 1, 1, 1
 * 1, 1, 2
 * 1, 3
 * 2, 2
 * 4
 *
 * This files provides two methods:
 *
 * - npart_create: creates an iterator over the partitions of an integer number.
 * - dual_npart_create: creates an iterator over the partitions of a tuple of integers.
 *
 * E.g.:
 *  ````
 *     group_iter_t *iter =  dual_npart_create(
 *        npart_create(4), npart_create(4))
 *  ````
 *  
 * results in an iterator for sweeping all the different ways of adding up
 * tuples of two numbers to (4, 4).  A few of the tuples could be:
 *      (1, 0), (1, 0), (1, 0) ... (0, 1)
 *      ...
 *      (1, 1), (1, 1), (1, 1) ... (1, 1)
 *      ...
 *      (4, 4)
 *      
 * Probably needs some refactoring but the interfaces are fine.
 */
struct group_iter_t {
  void (*begin)(struct group_iter_t *);
  int  (*next)(struct group_iter_t *);
  int  (*end)(struct group_iter_t const *);
  void (*free)(struct group_iter_t *);

  /* Returns the tuple format of the passed unsigned integer.
   *
   * For example, consider:
   *
   * ````
   * npart_dual_create(npart_create(4), npart_create(3))
   * ````
   *
   * The possible number of states is (4+1) * (3+1) = 20
   * A possible state could be: 12, 4, 3
   * A state of value 12, indicates the tuple (3, 0)
   * A state of value 4 , indicates the tuple (1, 0)
   * A state of value 3 , indicates the tuple (0, 3)
   *
   * The summation of the values in a state always equals the total - 1
   * (because 0 is also a state).
   */
  unsigned (*to_tuple)(struct group_iter_t const *, uint32_t, uint32_t *);

  /* Returns a number/state associated with a tuple value */
  uint32_t (*from_tuple)(struct group_iter_t *, unsigned, uint32_t *);

  /* Returns the number of subsets in a group */
  unsigned (*num_subsets)(struct group_iter_t const *);

  uint32_t *state;          /* Internal state of the iterator: each entry is a
                               "number" encoding the tuple of the underlying
                               npart */

  uint32_t state_length;    /* Number of tuples/numbers in this state */
  uint32_t total;           /* Total number of combination of "numbers" */
  uint32_t tuple_size;      /* A tuple is the tuple representation of a "state"
                               variable */
  uint32_t max_tuple_size;  /* Maximum size of the tuple returned */
};

/* Create a single iterator */
struct group_iter_t *npart_create(uint32_t n);

/* Create a composite iterator from either composite or single iterators (or a
 * combination of both) */
struct group_iter_t *dual_npart_create(struct group_iter_t *, struct group_iter_t *);

#endif
