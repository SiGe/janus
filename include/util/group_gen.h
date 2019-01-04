#ifndef _GROUP_GEN_H_
#define _GROUP_GEN_H_

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct group_iter_t {
  void (*begin)(struct group_iter_t *);
  int  (*next)(struct group_iter_t *);
  int  (*end)(struct group_iter_t *);
  void (*free)(struct group_iter_t *);
  int  (*to_tuple)(struct group_iter_t *, uint32_t, uint32_t *);
  int  (*num_subsets)(struct group_iter_t *);

  uint32_t *state;
  uint32_t state_length;
  uint32_t total;
  uint32_t tuple_size;
  uint32_t max_tuple_size;
};

/* TODO: Private data-structures.  Move to c file later. */
/* A0000041 */
struct npart_iter_state_t {
  struct group_iter_t;

  uint32_t state_min_allowed;
  uint32_t last_allowed;
  uint32_t finished;
};

struct group_iter_t *npart_create(uint32_t n);

/* TODO: XX */
struct dual_npart_iter_state_t {
  struct group_iter_t;

  struct group_iter_t *iter1, *iter2;

  // Indices of comp_2 associated with comp_1
  uint32_t *comp_index;
  // This is for comp1_indices
  uint32_t *comp_pointers;
  uint32_t comp_total;

  uint32_t *comp1, *comp2;
  uint32_t comp1_len, comp2_len;

  uint32_t last_class, last_index;
  uint32_t *min_class_index;

  // This is for comp2_indices
  uint32_t *avail;
  uint32_t avail_len;

  uint32_t finished;
};

// Shared so far:
// begin
// next
// end
//
// state
// state_length
// total

void dual_npart_state_current(struct dual_npart_iter_state_t *s);
struct group_iter_t *dual_npart_create(
    struct group_iter_t *iter1,
    struct group_iter_t *iter2);
void dual_npart_free(struct dual_npart_iter_state_t *iter);

#endif
