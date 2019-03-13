#ifndef _ALGO_ARRAY_H_
#define _ALGO_ARRAY_H_

#define ARRAY_MAGIC 0xdeadbeef

#include <stdlib.h>

// A contiguous chunk of data
//
// Does automatic resizing when it cannot fit data anymore
struct array_t {
  int  data_size;
  int  count;
  int  capacity;
  void *data;
};

struct array_t* array_create(int data_size, int capacity);
struct array_t* array_from_vals(void *data, int data_size, int size);
void            array_free(struct array_t *);
void     array_set(struct array_t *, void *data, int index);
void*    array_get(struct array_t *, int index);
void     array_append(struct array_t *, void *data);
int      array_size(struct array_t *);
int      array_capacity(struct array_t *);
int      array_transfer_ownership(struct array_t *, void **data);
void*    array_splice(struct array_t *, int start, int end, int *size);

char*           array_serialize(struct array_t *, size_t *size);
struct array_t* array_deserialize(char *data, size_t size);

#endif // _ALGO_ARRAY_H_
