#ifndef _ALGO_ARRAY_H_
#define _ALGO_ARRAY_H_

#include <stdint.h>
#include <stdlib.h>

// An ordered contiguous chunk of elements that does automatic resizing when
// there is no space left.  Uses exponential growth for O(1) insertion.
struct array_t {
  int  data_size;
  int  count;
  int  capacity;
  void *data;
};

// Create an array with a specific capacity or from a set of values
struct array_t* array_create(int data_size, int capacity);
struct array_t* array_from_vals(void *data, int data_size, int size);
void            array_free(struct array_t *);

// Get/Set/Append functions for array
void     array_set(struct array_t *, void *data, int index);
void*    array_get(struct array_t const*, int index);
void     array_append(struct array_t *, void *data);

// Capacity and size functions
inline int array_size(struct array_t const *arr) {
  return arr->count;
}

inline int array_capacity(struct array_t const *arr) {
  return arr->capacity;
}

// Transfers the ownership of data held by the array
int      array_transfer_ownership(struct array_t *, void **data);

// Returns a copy of the data from [start, end] (inclusive range)
void*    array_splice(struct array_t const *, int start, int end, int *size);

// Serialize and deserialize functions
char*           array_serialize(struct array_t const*, size_t *size);
struct array_t* array_deserialize(char const *data, size_t size);

#endif // _ALGO_ARRAY_H_
