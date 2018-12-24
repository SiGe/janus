#include <string.h>
#include <stdlib.h>

#include "util/common.h"
#include "util/log.h"
#include "traffic.h"

#define TM_SIZE(p) (p->num_pairs * sizeof(struct pair_bw_t) + sizeof(struct traffic_matrix_t))

void traffic_matrix_save(struct traffic_matrix_t *tm, FILE * f) {
  if (f == 0)
    panic("File pointer is null.");

  size_t size = sizeof(struct traffic_matrix_t) + tm->num_pairs * sizeof(struct pair_bw_t);
  info("Writing %llu to file.", size );
  int written = fwrite((void*)tm, size, 1, f);
  if (written != size) {
    if (ferror(f)) {
      panic("Couldn't dump the traffic matrix!");
    }
  }
  fflush(f);
}

struct traffic_matrix_t *traffic_matrix_load(FILE *f) {
  if (f == 0)
    panic("File pointer is null.");

  struct traffic_matrix_t tm = {0};
  size_t size = sizeof(struct traffic_matrix_t);
  int read = fread(&tm, 1, size, f);
  if (read != size) {
    if (feof(f)) 
      panic("Reached the end of file!");
    if (ferror(f))
      panic("Error reading from file.");
    panic("Error reading the file ...");
  }

  fseek(f, -size, SEEK_CUR);
  size = sizeof(struct traffic_matrix_t) + tm.num_pairs * sizeof(struct pair_bw_t);

  /* Read the file */
  struct traffic_matrix_t *ret = malloc(size);
  read = fread(ret, 1, size, f);

  if (read != size) {
    if (feof(f)) 
      panic("Reached the end of file!");
    if (ferror(f))
      panic("Error reading from file.");
    panic("Error reading the file : %d != %d", read, size);
  }

  return ret;
}

void traffic_matrix_free(struct traffic_matrix_t *tm) {
  if (!tm) return;

  free(tm);
}

/* Does not take the ownership of tm, so don't forget to free */
void traffic_matrix_trace_add(
    struct traffic_matrix_trace_t *trace,
    struct traffic_matrix_t *tm,
    trace_time_t key) {

  if (trace->num_indices == trace->cap_indices) {
    size_t size         = sizeof(struct traffic_matrix_trace_index_t) * trace->num_indices;
    trace->indices      = realloc(trace->indices, size * 2);
    trace->cap_indices *= 2;
  }

  struct traffic_matrix_trace_index_t *idx = &trace->indices[trace->num_indices];
  idx->size = TM_SIZE(tm);
  idx->time = key;

  fseek(trace->fdata, SEEK_SET, trace->largest_seek);
  traffic_matrix_save(tm, trace->fdata);

  if (trace->num_indices == 0) {
    idx->seek = 0;
  } else {
    idx->seek = trace->largest_seek;
    trace->largest_seek = idx->seek + idx->size;
  }

  trace->num_indices++;
};

static
int _compare_indices(void const* v1, void const *v2) {
  struct traffic_matrix_trace_index_t *t1 = (struct traffic_matrix_trace_index_t *)v1;
  struct traffic_matrix_trace_index_t *t2 = (struct traffic_matrix_trace_index_t *)v2;

  return (t1->time - t2->time);
}

static
void _traffic_matrix_trace_optimize(struct traffic_matrix_trace_t *trace) {
  if (trace->_optimized)
    return;

  qsort(trace->indices,
      trace->num_indices,
      sizeof(struct traffic_matrix_trace_index_t),
      _compare_indices);

  trace->_optimized = 1;
}

struct traffic_matrix_trace_index_t *_traffic_matrix_trace_get_idx(
    struct traffic_matrix_trace_t *trace, trace_time_t key) {
  int begin = 0, end = trace->num_indices-1;
  int mid = 0;

  while (1) {
    mid = (end + begin) / 2;
    struct traffic_matrix_trace_index_t *m = &trace->indices[mid];
    if (m->time < key) {
      begin = mid+1;
    } else if (m->time > key) {
      end = mid-1;
    } else {
      return m;
    }

    if (begin > end) 
      return 0;
  }
}

static
uint16_t _hash(trace_time_t key, uint16_t cache_size) {
  return key % cache_size;
}

struct traffic_matrix_t *_traffic_matrix_trace_get_key_in_cache(
    struct traffic_matrix_trace_t *trace,
    trace_time_t key) {
  struct traffic_matrix_trace_cache_t *cache =
    &trace->caches[_hash(key, trace->num_caches)];
  if (!cache->is_set)
    return 0;
  if (cache->time != key)
    return 0;
  return cache->tm;
}

static
void _traffic_matrix_trace_set_key_in_cache(
    struct traffic_matrix_trace_t *trace,
    trace_time_t key,
    struct traffic_matrix_t *tm
    ) {

  if (tm == 0) {
    return;
  }

  struct traffic_matrix_trace_cache_t *cache =
    &trace->caches[_hash(key, trace->num_caches)];
  if (cache->is_set) {
    info("Invalidating cache for key: %llu with hash %u", key, _hash(key, trace->num_caches));
    free(cache->tm);
    cache->tm = 0;
  }

  cache->is_set = 1;
  cache->time = key;
  cache->tm = tm;
}

void traffic_matrix_trace_get(
    struct traffic_matrix_trace_t *trace,
    trace_time_t key,
    struct traffic_matrix_t **tm) {

  // If the key is in cache
  struct traffic_matrix_t *t = 
    _traffic_matrix_trace_get_key_in_cache(trace, key);
  struct traffic_matrix_t *ret = 0;
  if (t) {
    ret = malloc(TM_SIZE(t));
    memcpy(ret, t, TM_SIZE(t));
    *tm = ret;
    return;
  }
  
  if (trace->_optimized == 0)
    _traffic_matrix_trace_optimize(trace);

  struct traffic_matrix_trace_index_t *index = 
    _traffic_matrix_trace_get_idx(trace, key);

  if (!index) {
    *tm = 0;
    return;
  }

  fseek(trace->fdata, SEEK_SET, index->seek);
  struct traffic_matrix_t *cache_obj = traffic_matrix_load(trace->fdata);
  _traffic_matrix_trace_set_key_in_cache(trace, key, cache_obj);

  ret = malloc(TM_SIZE(cache_obj));
  memcpy(ret, cache_obj, TM_SIZE(cache_obj));
  *tm = ret;
}

static
uint64_t _traffic_matrix_trace_index_count(FILE *index) {
  uint64_t size = 0;
  fseek(index, SEEK_SET, 0);
  fread(&size, sizeof(size), 1, index);
  return size;
}

struct traffic_matrix_trace_t *traffic_matrix_trace_create(
    uint16_t num_caches,
    uint16_t cap_indices, const char *name) {
  size_t index_size = sizeof(struct traffic_matrix_trace_index_t) * cap_indices;
  size_t cache_size = sizeof(struct traffic_matrix_trace_cache_t) * num_caches;
  size_t size = sizeof(struct traffic_matrix_trace_t);

  struct traffic_matrix_trace_t *trace = malloc(size);

  trace->indices = (struct traffic_matrix_trace_index_t *)malloc(index_size);
  trace->caches = (struct traffic_matrix_trace_cache_t *)malloc(cache_size);

  memset(trace->caches, 0, cache_size);
  memset(trace->indices, 0, index_size);

  trace->_optimized = 0;

  trace->num_caches = num_caches;

  // Zero indices at the moment
  trace->num_indices = 0;
  trace->cap_indices = cap_indices;
  trace->largest_seek = 0;

  if (name != 0) {
    char fname[PATH_MAX] = {0};
    char fdata[PATH_MAX] = {0};

    (void) strncat(fname, name, PATH_MAX - 1);
    (void) strncat(fname, ".index", PATH_MAX - 1);
    (void) strncat(fdata, name, PATH_MAX - 1);
    (void) strncat(fdata, ".data", PATH_MAX - 1);

    FILE *index = fopen(fname, "wb+");
    FILE *data = fopen(fdata, "wb+");

    trace->fdata = data;
    trace->findex = index;
  }

  return trace;
}

static
void _traffic_matrix_trace_load_indices(struct traffic_matrix_trace_t *trace) {
  fseek(trace->findex, SEEK_SET, 0 + sizeof(trace->num_indices));

  size_t size = sizeof(struct traffic_matrix_trace_index_t) * trace->num_indices;

  size_t nread = fread(trace->indices, 1, size, trace->findex); 
  if (nread != size) {
    panic("Couldn't load the indices.");
  }

  uint64_t num_indices = trace->num_indices;
  struct traffic_matrix_trace_index_t *index = trace->indices;
  for (uint64_t i = 0; i < num_indices; ++i) {
    uint64_t seek_idx = index->seek + index->size;
    if (seek_idx > trace->largest_seek) {
      trace->largest_seek = seek_idx;
    }
  }
}

static
void _traffic_matrix_trace_save_indices(struct traffic_matrix_trace_t *trace) {
  fseek(trace->findex, SEEK_SET, 0);
  size_t written = fwrite(&trace->num_indices, 1, sizeof(trace->num_indices), trace->findex);
  if (written != sizeof(trace->num_indices)) {
    panic("Couldn't save indices count: wrote %d instead of %d", written, sizeof(trace->num_indices));
  }
  size_t size = sizeof(struct traffic_matrix_trace_index_t) * trace->num_indices;
  written = fwrite(trace->indices, 1, size, trace->findex);
  if (written != size) {
    panic("Couldn't save indices: wrote %d instead of %d.", written, size);
  }
  fflush(trace->findex);
}

void traffic_matrix_trace_save(struct traffic_matrix_trace_t *trace) {
  // Anything else we need to do?
  _traffic_matrix_trace_save_indices(trace);
}

struct traffic_matrix_trace_t *traffic_matrix_trace_load(
    uint16_t num_caches, const char *name) {
  char fname[PATH_MAX] = {0};
  char fdata[PATH_MAX] = {0};

  (void) strncat(fname, name, PATH_MAX - 1);
  (void) strncat(fname, ".index", PATH_MAX - 1);
  (void) strncat(fdata, name, PATH_MAX - 1);
  (void) strncat(fdata, ".data", PATH_MAX - 1);

  FILE *index = fopen(fname, "ab+");
  FILE *data = fopen(fdata, "ab+");

  if (!index || !data) {
    panic("Couldn't find the associated index or data file.");
  }

  uint64_t indices = _traffic_matrix_trace_index_count(index);
  struct traffic_matrix_trace_t *trace = traffic_matrix_trace_create(num_caches, indices, 0);
  trace->fdata = data;
  trace->num_indices = indices;
  trace->findex = index;

  _traffic_matrix_trace_load_indices(trace);

  return trace;
}

void traffic_matrix_trace_print_index(struct traffic_matrix_trace_t *t) {
  for (uint64_t i = 0; i < t->num_indices; ++i) {
    struct traffic_matrix_trace_index_t *index = &t->indices[i];
    printf("indices are: %llu  @%llu [:%llu]\n", index->size, index->time, index->seek);
  }
}

void traffic_matrix_trace_free(struct traffic_matrix_trace_t *t) {
  struct traffic_matrix_trace_cache_t *cache = t->caches;
  for (uint16_t i = 0; i < t->num_caches; ++i) {
    if (cache->is_set) {
      free(cache->tm);
    }
    cache++;
  }

  fclose(t->fdata);
  fclose(t->findex);

  free(t->caches);
  free(t->indices);
  free(t);
}
