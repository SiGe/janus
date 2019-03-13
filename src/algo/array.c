#include <stdlib.h>
#include <string.h>

#include "util/log.h"

#include "algo/array.h"

struct array_t *array_create(int data_size, int capacity) {
  if (data_size <= 0 || capacity <= 0) {
    panic("Array data size or capacity are <= 0.");
    return 0;
  }

  struct array_t *ret = malloc(sizeof(struct array_t));
  ret->data_size = data_size;
  ret->capacity = capacity;
  ret->data = malloc(data_size * capacity);
  ret->count = 0;

  return ret;
}

struct array_t *array_from_vals(void *data, int data_size, int size) {
  if (data_size <= 0 || size <= 0) {
    panic("Array data size or capacity are <= 0.");
    return 0;
  }

  struct array_t *ret = malloc(sizeof(struct array_t));
  ret->data_size = data_size;
  ret->capacity = size;
  ret->count = size;
  ret->data = data;

  return ret;
}

void array_free(struct array_t *arr) {
  if (arr->data)
    free(arr->data);
  free(arr);
}

char *array_serialize(struct array_t *arr, size_t *out_size) {
  size_t data_size = (arr->count * arr->data_size);
  size_t size = data_size + sizeof(struct array_t);
  char *output = malloc(size);

  void *data = arr->data;

  // Save array magic in place of data
  arr->data = (void *)ARRAY_MAGIC;
  memcpy(output, arr, sizeof(struct array_t));
  memcpy(((char *)output) + sizeof(struct array_t), data, data_size);
  arr->data = data;
  *out_size = size;

  return output;
}

struct array_t *array_deserialize(char *bytes, size_t size) {
  if (size < sizeof(struct array_t)) {
    panic("Wrong array size: %d < %d (min array size)", size, sizeof(struct array_t));
    return 0;
  }
  
  struct array_t *output = malloc(sizeof(struct array_t));
  memcpy(output, bytes, sizeof(struct array_t));
  if (output->data != (void*)ARRAY_MAGIC) {
    panic("Wrong array magic.  Maybe you are loading the wrong thing?");
    return 0;
  }

  output->data = malloc(output->data_size * output->count);
  output->capacity = output->count;

  memcpy(output->data, bytes + sizeof(struct array_t), size - sizeof(struct array_t));
  return output;
}

inline void array_set(struct array_t *arr, void *val, int index) {
  void *data = array_get(arr, index);
  if (data == 0) {
    panic("Couldn't locate the location of "
          "the index to fill with data: %d < %d", 
          index, array_size(arr));
  }

  memcpy(data, val, arr->data_size);
}

inline void* array_get(struct array_t *arr, int index) {
  if (arr->count <= index)
    return 0;

  return ((char *)arr->data) + (arr->data_size * index);
}

inline void array_append(struct array_t *arr, void *data) {
  while (arr->capacity <= arr->count) {
    if (arr->capacity == 0)
      arr->capacity += 1;

    arr->capacity *= 2;
    arr->data = realloc(arr->data, arr->capacity * arr->data_size);
    if (!arr->data)
      panic("Could not reallocate the data to make space for array: %d, %d", arr->capacity, arr->data_size);
  }

  arr->count += 1;
  array_set(arr, data, arr->count - 1);
}

inline int array_size(struct array_t *arr) {
  return arr->count;
}

inline int array_capacity(struct array_t *arr) {
  return arr->capacity;
}

int array_transfer_ownership(struct array_t *arr, void **data) {
  *data = arr->data;
  int ret = arr->count;

  arr->count = 0;
  arr->capacity = 0;
  arr->data = 0;

  return ret;
}

void *array_splice(struct array_t *arr, int start, int end, int *size) {
  if (!arr)
    return 0;

  // Sanitization of input
  if (start < 0)
    start = 0;

  if (end >= array_size(arr))
    end = array_size(arr) - 1;

  size_t copy_size = (end - start + 1) * arr->data_size;
  void *ret = malloc(copy_size);
  void *data_start = (void *)((char *)arr->data + (arr->data_size * start));
  memcpy(ret, data_start, copy_size);

  *size = end - start + 1;
  return ret;
}
