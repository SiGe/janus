#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "util/log.h"

#include "algo/group_gen.h"

#define max(a, b) ((a) > (b)) ? (a) : (b)

/* A000041 */
struct npart_iter_state_t {
  struct group_iter_t;

  uint32_t state_min_allowed;
  uint32_t last_allowed;
  uint32_t finished;
};

/* TODO: XX */
struct dual_npart_iter_state_t {
  struct group_iter_t;

  struct group_iter_t *iter1, *iter2;

  // State for first iterator
  uint32_t *comp_pointers;
  uint32_t comp_total;

  uint32_t *comp1;
  uint32_t comp1_len;

  uint32_t last_class, last_index;
  uint32_t *min_class_index;

  // State for second iterator
  uint32_t *avail;
  uint32_t avail_len;

  // Indices for mapping the first iterator to the second
  uint32_t *comp_index;

  uint32_t finished;
};

static inline
int _npart_state_num_subsets(
    struct group_iter_t const *in) {
  struct npart_iter_state_t *s = 
    (struct npart_iter_state_t *)in;

  return (s->total + 1);
}

static inline
int _npart_state_to_tuple(
    struct group_iter_t const *in,
    uint32_t val,
    uint32_t *ret) {
  return *ret = val;
}

static inline
uint32_t _npart_state_from_tuple(
    struct group_iter_t *in,
    int len,
    uint32_t *ret) {
  return *ret;
}

void _npart_begin(struct group_iter_t *s) {
  struct npart_iter_state_t *out = (struct npart_iter_state_t *)s;
  memset(out->state, 0, sizeof(uint32_t) * out->total);
  out->state_min_allowed = 0;
  out->state_length = 0;
  out->last_allowed = 1;
  out->finished = 0;

  // Initiate the first state
  out->next((struct group_iter_t *)out);
}

void _npart_free(struct group_iter_t *in) {
  struct npart_iter_state_t *s = (struct npart_iter_state_t *)in;
  free(s->state);
  free(s);
}

int _npart_end(struct group_iter_t const *in) {
  struct npart_iter_state_t *s = (struct npart_iter_state_t *)in;
  return s->finished;
}

// TODO: Probably can clean this up ... there is a possibility that we can
// merge state_min_allowed and last_allowed but too lazy atm to get it working.
int _npart_next(struct group_iter_t *in) {
  struct npart_iter_state_t *s = (struct npart_iter_state_t *)in;
  // If we are done ... well, we are done.
  uint32_t ma  = s->state_min_allowed;
  uint32_t *last = &s->state[s->state_length - 1];
  uint32_t *prev = &s->state[s->state_length - 2];

  if (ma == s->total) {
    s->finished = 1;
    return 0;
  }

  // state == [ma total-ma]
  if (s->state_length <= 2) {
    // We have almost gathered everything back.
    // go one more round by gathering everything and spreading it back
    s->state_min_allowed += 1;
    uint32_t count = s->total / s->state_min_allowed;
    uint32_t remainder = s->total - (count * (s->state_min_allowed));
    uint32_t last = 0; 

    if (remainder == 0) {
      // count = 9 / 3 = 3
      // remainder = 0
      // [3 3] + [3]
      last = s->state_min_allowed;
      // The minus one to count here is because we will adjust for it later
      // by setting the last value.
      count -= 1; 
    } else { // if (remainder < s->state_min_allowed)
      // Adjust the count.
      // count = 10 / 3 = 3
      // remainder = 1
      // [3 3] + [4]
      last = remainder + s->state_min_allowed;
      count -= 1;
    }

    for (uint32_t i = 0; i < count; ++i) {
      s->state[i] = s->state_min_allowed;
    }
    s->state[count] = last;
    s->state_length = count + 1;
    if (count == 0)
      s->state_min_allowed = s->total;
    return 1;
  }


  // This [1 1 2 2] cannot be split into
  //      [1 1 3 1 ...] next of [1 1 2 2] is [1 1 4]
  if (prev + ma >= last) {
    s->last_allowed = *prev + 1;
    *prev = *prev + *last;
    *last = 0;
    s->state_length -= 1;

    uint32_t div = *prev / (s->last_allowed);
    uint32_t rem = *prev - (div * s->last_allowed);
    uint32_t last = 0;

    if (div == 0)
      return 1;

    if (div == 1) {
      s->last_allowed -= 1;
      return 1;
    }

    if (rem == 0) {
      // expand w/e we have available
      div -= 1;
      last = s->last_allowed;
    } else { // if rem < s->last_allowed ....
      div -= 1;
      last = rem + s->last_allowed;
    }

    for (uint32_t i = 0; i < div; ++i) {
      *(prev + i) = s->last_allowed;
    }
    *(prev + div) = last;
    s->state_length += div;
    return 1;
  }

  // else [1 1 2 4], we can take away from the last and give it to the prev that is:
  // next([1 1 2 4]) = [1 1 3 3]
  *prev += ma;
  *last -= ma;

  return 1;
}

struct group_iter_t *npart_create(uint32_t n) {
  struct npart_iter_state_t *out = malloc(
      sizeof(struct npart_iter_state_t));

  out->state = malloc(sizeof(uint32_t) * n);
  out->total = n;
  out->begin = _npart_begin;
  out->next  = _npart_next;
  out->free  = _npart_free;
  out->end   = _npart_end;
  out->to_tuple = _npart_state_to_tuple;
  out->from_tuple = _npart_state_from_tuple;
  out->num_subsets = _npart_state_num_subsets;

  out->tuple_size = 1;

  _npart_begin((struct group_iter_t *)out);

  return (struct group_iter_t *)out;
}

static inline
int _find_alloc_index(
    struct dual_npart_iter_state_t *s,
    uint32_t min_index) {
  int32_t avail_index = -1;
  for (int32_t i = max(min_index, 1); i < s->avail_len; ++i) {
    if (s->avail[i] > 0) {
      s->avail[i] -= 1;
      avail_index = i;
      break;
    }
  }
  return avail_index;
}
    
static inline
void _release_index(
    struct dual_npart_iter_state_t *s,
    uint32_t index) {
  if (index > s->avail_len) {
    panic("Trying to release an invalid index: %d (should be <%d)", index, s->avail_len);
    return;
  }
  s->avail[index] += 1;
}

void _find_next_comb_for_class(
    struct dual_npart_iter_state_t *s, uint32_t class, uint32_t at) {

  // Find the next available index for our class that is greater than
  // min_class_index; 
  uint32_t ma = s->min_class_index[class];
  uint32_t len = (s->comp_pointers[class+1] - s->comp_pointers[class]);

  // If no element here ... move on
  if (len == 0) {
    s->min_class_index[class] = s->avail_len;
    return;
  }

  if (len == 1) {
    uint32_t *last = &s->comp_index[s->comp_pointers[class]];
    // try to find an index that is greater than min_class_index, and if we
    // couldn't find one just mark the min_class_index to be equal to avail_len
    // to indicate that we don't have space left
    
    // Find an available index that is greater than or equal to
    // its current index
    int32_t avail_index = _find_alloc_index(s, *last + 1);
    _release_index(s, *last);
    *last = avail_index;

    s->min_class_index[class] = avail_index;
    if (avail_index == -1) {
      s->min_class_index[class] = s->avail_len;
      *last = s->avail_len;
    }
    return;
  }

  // If there are more than one element in the class
  // try to find a combination that works
  uint32_t *begin = &s->comp_index[s->comp_pointers[class]];
  uint32_t *end   = &s->comp_index[s->comp_pointers[class+1]];
  uint32_t *prev  = end - 1;

  // next index we find should be greater than
  assert(*prev >= ma);

  // We have to assign indices to all the values in between prev and last
  // with the condition that all the indices point to a value higher than
  // *prev.
  
  while (1) {
    int failed = 0;
    uint32_t *end_search = 0;
    uint32_t min_index = *prev;
    for (uint32_t *idx = prev; idx < end; idx++) {
      int32_t avail_index = _find_alloc_index(s, min_index + 1);
      if (avail_index == -1) {
        // If we couldn't find an index that means that we can stop the search
        // for a feasible assignment and start with a lower index
        failed = 1;
        end_search = idx;
        break;
      }

      // if we found an available index, just set it and try to set
      // the next index value
      _release_index(s, *idx);
      *idx = avail_index;
    }

    if (failed == 0) {
      // TODO: We found a matching assignment
      // I think we can return safely here?
      return;
    }

    // If we failed to find a good assignment:
    // First recover all the indices we tried to use but failed.
    for (uint32_t *idx = prev; idx < (end_search + 1); idx++) {
      _release_index(s, *idx);
      *idx = 0;
    }

    // Then try to search in a larger range.
    // BUT, if we are at the beginning don't go back any further
    int32_t avail_index = -1;
    while (avail_index == -1 && prev != begin) {
      prev -= 1;
      // Try to find an empty index for our new prev
      avail_index = _find_alloc_index(s, *prev + 1);

      // If we couldn't find an index (which shouldn't happen because we
      // released everything a few lines up here (unless *prev + 1 is greater
      // than s->avail_len).
      if (avail_index != -1)
        _release_index(s, avail_index);
    }


    // If we couldn't find an index just return and mention that we couldn't
    // find an assignment
    if (avail_index == -1) {
      s->min_class_index[class] = s->avail_len;
      return;
    }

    // And also update the min_class_index ...
    if (prev == begin) {
      s->min_class_index[class] = avail_index;
    }
  }
}

static
void _dual_npart_state_build(
    struct dual_npart_iter_state_t *s) {
    // Try to extract the state.
    uint32_t *st = s->state;
    uint32_t *stl = &s->state_length; *stl = 0;

    uint32_t t1 = s->iter1->total;
    uint32_t t2 = s->iter2->total;

    (void)t1;

    for (uint32_t i = 1; i < s->comp1_len; ++i) {
      for (uint32_t j = s->comp_pointers[i]; j < s->comp_pointers[i+1]; ++j) {
        uint32_t index = s->comp_index[j];
        *st++ = (i * (t2+1)) + index; /* +1 is to consider empty sets */
        *stl += 1;
      }
    }

    for (uint32_t i = 1; i < s->avail_len; ++i) {
      for( uint32_t j = 0; j < s->avail[i]; ++j) {
        *st++ = i;
        *stl += 1;
      }
    }
}


void _setup_for_next_iter(struct dual_npart_iter_state_t *s) {
  // TODO: Move this to begin()
  // Composite index is an index of all the cross combinations
  // that can happen.
  uint32_t index = 0;
  for (uint32_t i = 0; i < s->comp1_len; ++i) {
    s->comp_pointers[i] = index;
    for (uint32_t j = 0; j < s->comp1[i]; ++j) {
      s->comp_index[index] = 0;
      index++;
    }
  }
  s->comp_pointers[s->comp1_len] = index;
  s->comp_total = index;
}

static inline
int _dual_npart_next_in_class(struct dual_npart_iter_state_t *s) {
  while (1) {
    // Find next available position 
    _find_next_comb_for_class(s, s->last_class, 0);

    // If we have reached a class boundary ... increment the next class
    // and zero this one out.
    if (s->min_class_index[s->last_class] == s->avail_len) {
      // Reset the previous classes ...
      // We prob don't need to do this only for s->last_class [-1]
      for (uint32_t i = 0; i <= s->last_class; ++i) {
        s->min_class_index[i] = 0;
        for (uint32_t j = s->comp_pointers[i]; j < s->comp_pointers[i+1]; ++j) {
          // Recover the index
          uint32_t val = s->comp_index[j];
          _release_index(s, val);
          s->comp_index[j] = 0; // Attach the index to empty set
        }
      }

      // Move on to the next class
      s->last_class += 1;
      if (s->last_class == s->comp1_len) {
        return 0;
      }

    } else {
      // increment the current class and set the last_class to zero
      // Reset?
      s->last_class = 0;
      return 1;
    }
  }
}

void _prepare_comps(struct dual_npart_iter_state_t *s) {
  uint32_t *s1 = s->iter1->state;
  uint32_t s1l = s->iter1->state_length;
  uint32_t *s2 = s->iter2->state;
  uint32_t s2l = s->iter2->state_length;

  size_t size1 = (s->iter1->total+2) * sizeof(uint32_t);
  size_t size2 = (s->iter2->total+2) * sizeof(uint32_t);

  memset(s->comp1, 0, size1);
  memset(s->avail, 0, size2);
  memset(s->min_class_index, 0, size1);
  memset(s->comp_index, 0, sizeof(uint32_t) * (s1l+2));
  memset(s->comp_pointers, 0, sizeof(uint32_t) * (s1l+2));

  s->comp1_len = 0;
  s->last_index = 0;
  s->last_class = 0;

  for (uint32_t i = 0; i < s1l; ++i) {
    s->comp1[s1[i]] += 1;
    s->comp1_len = max(s->comp1_len, s1[i]);
  }

  for (uint32_t i = 0; i < s2l; ++i) {
    s->avail_len = max(s->avail_len, s2[i]);
    s->avail[s2[i]] += 1;
  }

  s->comp1_len += 1;
  s->avail_len += 1;

  _setup_for_next_iter(s);
}

static
void _dual_npart_free(struct group_iter_t *in) {
  struct dual_npart_iter_state_t *iter =
    (struct dual_npart_iter_state_t *)in;

  free(iter->comp_index);
  free(iter->comp_pointers);
  free(iter->comp1);
  free(iter->min_class_index);
  free(iter->avail);
  free(iter->state);

  free(iter);
}

static
int _dual_npart_state_next(struct group_iter_t *in) {
  struct dual_npart_iter_state_t *s =
    (struct dual_npart_iter_state_t *)in;
  while (1) {
    uint32_t success = _dual_npart_next_in_class(s);

    if (success == 1) {
      _dual_npart_state_build(s);
      return 1;
    }

    // If we have finished iterating---move on.
    s->iter1->next(s->iter1);
    if (s->iter1->end(s->iter1)) {
      s->iter1->begin(s->iter1);
      s->iter2->next(s->iter2);
      if (s->iter2->end(s->iter2)) {
        // we are done ... :)
        s->finished = 1;
        return 0;
      }
    }

    _prepare_comps(s);
    _dual_npart_state_build(s);
    return 1;
  }


  // If we have looked at every class in comp1_len, we are done.
  if (s->last_class == s->comp1_len) {
    s->finished = 1;
    return 0;
  }

  _dual_npart_state_build(s);
  return 1;
}

static
void _dual_npart_state_begin(
    struct group_iter_t *in) {
  struct dual_npart_iter_state_t *iter =
    (struct dual_npart_iter_state_t *)in;

  iter->iter1->begin(iter->iter1);
  iter->iter2->begin(iter->iter2);
  iter->finished = 0;

  _prepare_comps(iter);
  _dual_npart_state_build(iter);
}

static inline
int _dual_npart_state_num_subsets(
    struct group_iter_t const *in) {
  struct dual_npart_iter_state_t *s = 
    (struct dual_npart_iter_state_t *)in;

  return s->iter1->num_subsets(s->iter1) *
    s->iter2->num_subsets(s->iter2);
}

static inline
int _dual_npart_state_to_tuple(
    struct group_iter_t const *in,
    uint32_t val,
    uint32_t *ret) {

  struct dual_npart_iter_state_t *s = 
    (struct dual_npart_iter_state_t *)in;

  int ns2 = s->iter2->num_subsets(s->iter2);

  uint32_t val1 = val / ns2;
  uint32_t val2 = val - val1 * ns2;

  s->iter1->to_tuple(
      s->iter1, val1, ret);

  s->iter2->to_tuple(
      s->iter2, val2, ret + s->iter1->tuple_size);

  return s->tuple_size;
}

static inline
uint32_t _dual_npart_state_from_tuple(
    struct group_iter_t *in,
    int len,
    uint32_t *tuple) {

  struct dual_npart_iter_state_t *s = 
    (struct dual_npart_iter_state_t *)in;

  int ts1 = s->iter1->tuple_size;
  int ts2 = s->iter2->tuple_size;

  assert(len == ts1 + ts2);

  uint32_t r1 = s->iter1->from_tuple(
      s->iter1, ts1, tuple);

  uint32_t r2 = s->iter2->from_tuple(
      s->iter2, ts2, tuple + ts1);

  int ns2 = s->iter2->num_subsets(s->iter2);
  return r1 * ns2 + r2;
}


static
int _dual_npart_state_end(
    struct group_iter_t const *in) {
  struct dual_npart_iter_state_t *iter =
    (struct dual_npart_iter_state_t *)in;
  return (iter->finished);
}



struct group_iter_t *dual_npart_create(
    struct group_iter_t *iter1,
    struct group_iter_t *iter2) {

  struct dual_npart_iter_state_t *iter = malloc(
      sizeof(struct dual_npart_iter_state_t));

  iter1->begin(iter1);
  iter2->begin(iter2);

  size_t size1 = (iter1->total+2) * sizeof(uint32_t);
  size_t size2 = (iter2->total+2) * sizeof(uint32_t);

  iter->comp_index = malloc(size1);
  iter->tuple_size = iter1->tuple_size + iter2->tuple_size;

  // TODO: this could be sqrt(iter1->total?)
  // The + 1 is because we also keep an additional (empty) class at the end (to indicate the end)
  iter->comp_pointers= malloc(size1);

  iter->comp1 = malloc(size1);
  iter->min_class_index = malloc(size1);
  iter->avail = malloc(size2);

  // Max state is gonna be the sum of two
  iter->state = malloc(size1 + size2);
  iter->total = (iter1->total + 1) * (iter2->total + 1);

  iter->iter1 = iter1;
  iter->iter2 = iter2;

  iter->free = _dual_npart_free;
  iter->begin = _dual_npart_state_begin;
  iter->next = _dual_npart_state_next;
  iter->end = _dual_npart_state_end;
  iter->to_tuple = _dual_npart_state_to_tuple;
  iter->from_tuple = _dual_npart_state_from_tuple;
  iter->num_subsets = _dual_npart_state_num_subsets;

  _dual_npart_state_begin((struct group_iter_t *)iter);

  return (struct group_iter_t *)iter;
}
