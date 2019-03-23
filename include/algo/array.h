#ifndef _ALGO_ARRAY_H_
#define _ALGO_ARRAY_H_

#include <stdint.h>
#include <stdlib.h>

/** 
 * An ordered contiguous chunk of elements of size data_size.  array_t does
 * automatic resizing when there is no space left.  Uses exponential growth for
 * O(1) insertion.
 */
struct array_t {
  size_t data_size;
  unsigned count;
  unsigned capacity;
  void *data;
};

/** Create an array with a specific capacity or from a set of values */
struct array_t* array_create(size_t data_size, unsigned capacity);
struct array_t* array_from_vals(void *data, size_t data_size, unsigned size);
void            array_free(struct array_t *);

/** Set a data element at the specified index */
void     array_set(struct array_t *, void *data, unsigned index);

/** Get a data element from the specified index */
void*    array_get(struct array_t const*, unsigned index);

/** Append a piece of data to the end of the array */
void     array_append(struct array_t *, void *data);

/* Returns the number of elements in the array */
inline size_t array_size(struct array_t const *arr) {
  return arr->count;
}

/* Returns the capacity of the array */
inline unsigned array_capacity(struct array_t const *arr) {
  return arr->capacity;
}

/* Transfers the ownership of all data held by the array */
unsigned array_transfer_ownership(struct array_t *, void **data);

/* Returns a copy of the data from [start, end] (inclusive range) */
void* array_splice(struct array_t const *, unsigned start,
    unsigned end, unsigned *size);

/* Serialize and deserialize functions */
char*           array_serialize(struct array_t const*, size_t *size);
struct array_t* array_deserialize(char const *data, size_t size);

#endif // _ALGO_ARRAY_H_
