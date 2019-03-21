#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "util/common.h"
#include "util/log.h"
#include "traffic.h"

#define TM_SIZE(p) (p->num_pairs * sizeof(struct pair_bw_t) + sizeof(struct traffic_matrix_t))

void _tmti_begin(struct traffic_matrix_trace_iter_t *iter) {
    iter->state = iter->_begin;
}

int _tmti_next(struct traffic_matrix_trace_iter_t *iter) {
    iter->state += 1;
    if (iter->state > iter->_end) {
        iter->state = iter->_end;
        return 0;
    }
    return 1;
}

unsigned _tmti_length(struct traffic_matrix_trace_iter_t *iter) {
  return (iter->_end - iter->_begin) + 1;
}

struct traffic_matrix_t *_tmti_get_nocopy(struct traffic_matrix_trace_iter_t *iter) {
  panic_txt("TMTI doesn't support copy semantics. Use get.");
  return 0;
}

void _tmti_go_to(struct traffic_matrix_trace_iter_t *iter, trace_time_t time) {
  for (uint32_t i = 0; i < iter->trace->num_indices; ++i) {
    trace_time_t cur = 0;
    traffic_matrix_trace_get_nth_key(iter->trace, i, &cur);
    if (cur == time) {
      iter->state = i;
      return;
    }
  }

  iter->state = iter->_end;
}

int _tmti_end(struct traffic_matrix_trace_iter_t *iter) {
    return iter->state >= iter->_end; 
}

void _tmti_get(struct traffic_matrix_trace_iter_t *iter, struct traffic_matrix_t **tm) {
    trace_time_t time;
    if (traffic_matrix_trace_get_nth_key(iter->trace, iter->state, &time) != SUCCESS) {
        *tm = 0;
        return;
    }
    traffic_matrix_trace_get(iter->trace, time, tm);
}

void _tmti_free(struct traffic_matrix_trace_iter_t *iter) {
    free(iter);
}

static struct traffic_matrix_trace_iter_t *_tmt_iter(
        struct traffic_matrix_trace_t *trace) {
    struct traffic_matrix_trace_iter_t *iter = malloc(sizeof(struct traffic_matrix_trace_iter_t));
    iter->state = 0;
    iter->trace = trace;
    iter->begin = _tmti_begin;
    iter->go_to = _tmti_go_to;
    iter->next  = _tmti_next;
    iter->end   = _tmti_end;
    iter->get   = _tmti_get;
    iter->get_nocopy = _tmti_get_nocopy;
    iter->free  = _tmti_free;
    iter->length = _tmti_length;

    iter->_begin = 0;
    iter->_end = trace->num_indices;

    return iter;
}

void trace_iterator_set_range(struct traffic_matrix_trace_iter_t *iter, 
    uint32_t begin, uint32_t end) {
  if (end == 0)
    end = iter->trace->num_indices;

  iter->_begin = begin;
  iter->state = iter->_begin;
  iter->_end = end;
}

void traffic_matrix_save(struct traffic_matrix_t *tm, FILE * f) {
  assert(tm->num_pairs != 0);
  if (f == 0)
    panic_txt("File pointer is null.");

  size_t size = sizeof(struct traffic_matrix_t) + 
    tm->num_pairs * sizeof(struct pair_bw_t);

  // XXX: Bad idea: if the data-structures change we cannot just "read" the files anymore.
  //      Better way is to properly read and write the relevant parts by "hand."
  int written = fwrite((void*)tm, size, 1, f);
  if (written != size) {
    if (ferror(f)) {
      panic_txt("Couldn't dump the traffic matrix!");
    }
  }

  fflush(f);
}

struct traffic_matrix_t *traffic_matrix_load(FILE *f) {
  if (f == 0)
    panic_txt("File pointer is null.");

  struct traffic_matrix_t tm = {0};
  size_t size = sizeof(struct traffic_matrix_t);
  // XXX: Bad idea: if the data-structures change we cannot just "read" the files anymore
  //      Better way is to properly read and write the relevant parts by "hand."
  int read = fread(&tm, 1, size, f);
  if (read != size) {
    if (feof(f)) 
      panic("Reached the end of file! Tried to read %d got %d", size, read);
    if (ferror(f))
      panic_txt("Error reading from file.");
    panic_txt("Error reading the file ...");
  }

  long seek = -((long)size);
  fseek(f, seek, SEEK_CUR);
  size = sizeof(struct traffic_matrix_t) + tm.num_pairs * sizeof(struct pair_bw_t);

  /* Read the file */
  struct traffic_matrix_t *ret = malloc(size);
  // XXX: Bad idea: if the data-structures change we cannot just "read" the files anymore
  //      Better way is to properly read and write the relevant parts by "hand."
  read = fread(ret, 1, size, f);

  if (read != size) {
    if (feof(f)) 
      panic("Reached the end of file! "
          "Tried to read %d got %d (num_pairs = %d)", 
          size, read, tm.num_pairs);
    if (ferror(f))
      panic_txt("Error reading from file.");
    panic("Error reading the file : %d != %d", read, size);
  }

  return ret;
}

void traffic_matrix_free(struct traffic_matrix_t *tm) {
  if (!tm) return;

  free(tm);
}

struct traffic_matrix_t *traffic_matrix_multiply(
  bw_t value, struct traffic_matrix_t const *left) {
  pair_id_t num_pairs = left->num_pairs;
  if (!left)
     return 0;

  struct traffic_matrix_t *output = malloc(
      sizeof(struct pair_bw_t) * num_pairs);
  output->num_pairs = num_pairs;

  struct pair_bw_t const *pleft  = left->bws;
  struct pair_bw_t *pout = output->bws;

  for (pair_id_t pair_id = 0; pair_id < num_pairs; ++pair_id) {
    pout->bw = pleft->bw * value;
    pout++; pleft++;
  }

  return output;
}


struct traffic_matrix_t *traffic_matrix_add(
  struct traffic_matrix_t const *left, 
  struct traffic_matrix_t const *right) {
  if (!left || !right)
     return 0;

  if (left->num_pairs != right->num_pairs)
    return 0;

  pair_id_t num_pairs = left->num_pairs;

  struct traffic_matrix_t *output = malloc(
      sizeof(struct traffic_matrix_t) +
      sizeof(struct pair_bw_t) * num_pairs);

  output->num_pairs = num_pairs;

  struct pair_bw_t const *pleft  = left->bws;
  struct pair_bw_t const *pright = right->bws;
  struct pair_bw_t *pout = output->bws;

  for (pair_id_t pair_id = 0; pair_id < num_pairs; ++pair_id) {
    pout->bw = pleft->bw + pright->bw;
    pout++; pleft++; pright++;
  }

  return output;
}


/* Does not take the ownership of tm, so don't forget to free */
void traffic_matrix_trace_add(
    struct traffic_matrix_trace_t *trace,
    struct traffic_matrix_t *tm,
    trace_time_t key) {

  struct traffic_matrix_t *tm_exists = 0;
  traffic_matrix_trace_get(trace, key, &tm_exists);
  if (tm_exists)
    panic("Cannot add a TM that already has an associated key (%d).", key);

  if (trace->num_indices == trace->cap_indices) {
    trace->cap_indices *= 2;
    size_t size         = sizeof(struct traffic_matrix_trace_index_t) * trace->cap_indices;
    trace->indices      = realloc(trace->indices, size);
  }

  struct traffic_matrix_trace_index_t *idx = &trace->indices[trace->num_indices];
  idx->size = TM_SIZE(tm);
  idx->time = key;

  fseek(trace->fdata, (long)trace->largest_seek, SEEK_SET);
  traffic_matrix_save(tm, trace->fdata);

  if (trace->num_indices == 0) {
    idx->seek = 0;
    trace->largest_seek = idx->size;
  } else {
    idx->seek = trace->largest_seek;
    trace->largest_seek = idx->seek + idx->size;
  }

  trace->_optimized = 0; // Not optimized anymore (index is not sorted).
  trace->num_indices++;
}

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
  if ((trace->num_indices) == 0)
    return 0;

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

static inline
uint16_t _hash(trace_time_t key, uint16_t cache_size) {
  // Have to deal with negative keys
  return key & (cache_size - 1);
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
    //info("Invalidating cache for key: %llu with hash %u", key, _hash(key, trace->num_caches));
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

  // Move to that location in the file
  fseek(trace->fdata, (long)index->seek, SEEK_SET);
  struct traffic_matrix_t *cache_obj = traffic_matrix_load(trace->fdata);
  _traffic_matrix_trace_set_key_in_cache(trace, key, cache_obj);

  ret = malloc(TM_SIZE(cache_obj));
  memcpy(ret, cache_obj, TM_SIZE(cache_obj));
  *tm = ret;
}

static
uint64_t _traffic_matrix_trace_index_count(FILE *index) {
  uint64_t size = 0;
  fseek(index, 0, SEEK_SET);
  // XXX: Bad idea: if the data-structures change we cannot just "read" the files anymore
  //      Better way is to properly read and write the relevant parts by "hand."
  (void) fread(&size, sizeof(size), 1, index);
  return size;
}

struct traffic_matrix_trace_t *traffic_matrix_trace_create(
    uint16_t num_caches,
    uint16_t cap_indices, const char *name) {
  num_caches = upper_pow2(num_caches);
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
  trace->iter = _tmt_iter;

  if (name != 0) {
    char fname[PATH_MAX] = {0};
    char fdata[PATH_MAX] = {0};

    (void) strncat(fname, name, PATH_MAX - 1);
    (void) strncat(fname, ".index", PATH_MAX - 1);
    (void) strncat(fdata, name, PATH_MAX - 1);
    (void) strncat(fdata, ".data", PATH_MAX - 1);

    FILE *index = fopen(fname, "wb+");
    FILE *data = fopen(fdata, "wb+");

    if (!index || !data) {
      panic_txt("Couldn't create the associated index or data file.");
    }

    trace->fdata = data;
    trace->findex = index;
  }

  return trace;
}

static
void _traffic_matrix_trace_load_indices(struct traffic_matrix_trace_t *trace) {
  fseek(trace->findex, 0 + sizeof(trace->num_indices), SEEK_SET);

  size_t size = sizeof(struct traffic_matrix_trace_index_t) * trace->num_indices;

  // XXX: Bad idea: if the data-structures change we cannot just "read" the files anymore
  //      Better way is to properly read and write the relevant parts by "hand."
  size_t nread = fread(trace->indices, 1, size, trace->findex); 
  if (nread != size) {
    panic_txt("Couldn't load the indices.");
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
  fseek(trace->findex, 0, SEEK_SET);
  // XXX: Bad idea: if the data-structures change we cannot just "read" the files anymore
  //      Better way is to properly read and write the relevant parts by "hand."
  size_t written = fwrite(&trace->num_indices, 1, sizeof(trace->num_indices), trace->findex);
  if (written != sizeof(trace->num_indices)) {
    panic("Couldn't save indices count: wrote %d instead of %d", written, sizeof(trace->num_indices));
  }
  size_t size = sizeof(struct traffic_matrix_trace_index_t) * trace->num_indices;
  // XXX: Bad idea: if the data-structures change we cannot just "read" the files anymore
  //      Better way is to properly read and write the relevant parts by "hand."
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

  info("Trace: %.40s and %.40s files.", fdata, fname);

  if (!index || !data) {
    panic_txt("Couldn't find the associated index or data file.");
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
    //struct traffic_matrix_trace_index_t *index = &t->indices[i];
    //printf("indices are: %lu  @%lu [:%lu]\n", index->size, index->time, index->seek);
  }
}

void traffic_matrix_trace_for_each(struct traffic_matrix_trace_t *t,
    int (*exec)(struct traffic_matrix_t *, trace_time_t time, void *), void *metadata) {
  _traffic_matrix_trace_optimize(t);
  struct traffic_matrix_t *tm = 0;

  for (uint64_t i = 0; i < t->num_indices; ++i) {
    struct traffic_matrix_trace_index_t *index = &t->indices[i];
    traffic_matrix_trace_get(t, index->time, &tm);

    if (!tm)
      panic("TM is null.  Expected a TM for key: %d", index->time);

    if (!exec(tm, index->time, metadata))
      break;
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

struct traffic_matrix_t *traffic_matrix_zero(pair_id_t num_pairs) {
  size_t size = sizeof(struct traffic_matrix_t) +
      sizeof(struct pair_bw_t) * num_pairs;
  struct traffic_matrix_t *out = malloc(size);
  memset(out, 0, size);
  out->num_pairs = num_pairs;
  return out;
}

int traffic_matrix_trace_get_nth_key(
    struct traffic_matrix_trace_t *trace,
    uint32_t index, trace_time_t *ret) {
  if (index >= trace->num_indices)
    return FAILURE;

  if (!trace->_optimized)
    _traffic_matrix_trace_optimize(trace);

  *ret =trace->indices[index].time;
  return SUCCESS;
}

#define TO_TTIT(x) struct traffic_matrix_trace_iter_tms_t *ttit = (struct traffic_matrix_trace_iter_tms_t *)(x)

void _ttit_begin(struct traffic_matrix_trace_iter_t *iter) {
  iter->state = 0;
}

int _ttit_end(struct traffic_matrix_trace_iter_t *iter) {
  TO_TTIT(iter);
  return iter->state >= ttit->tms_length;
}

int _ttit_next(struct traffic_matrix_trace_iter_t *iter) {
  TO_TTIT(iter);
  iter->state += 1;
  if (iter->state >= ttit->tms_length)
    iter->state = ttit->tms_length;
  return 1;
}

void _ttit_get(struct traffic_matrix_trace_iter_t *iter, 
    struct traffic_matrix_t **tm) {
  TO_TTIT(iter);

  /* Lose ownership of the TM here */
  *tm = ttit->tms[ttit->state];
  ttit->tms[ttit->state] = 0;

  if (*tm == 0) {
    assert(0);
    panic_txt("Requesting a TM that we do not own anymore.");
  }
}

struct traffic_matrix_t *_ttit_get_nocopy(struct traffic_matrix_trace_iter_t *iter) {
  TO_TTIT(iter);
  return ttit->tms[ttit->state];
}

void _ttit_free(struct traffic_matrix_trace_iter_t *iter) {
  free(iter);
}

unsigned _ttit_length(struct traffic_matrix_trace_iter_t *iter) {
  TO_TTIT(iter);
  return ttit->tms_length;
}

void _ttit_go_to(struct traffic_matrix_trace_iter_t *iter, trace_time_t idx) {
  TO_TTIT(iter);
  ttit->state = idx;
  if (ttit->state > ttit->tms_length)
    ttit->state = ttit->tms_length;
}

struct traffic_matrix_trace_iter_t *traffic_matrix_iter_from_tms(
    struct traffic_matrix_t **tm, uint32_t size) {
  struct traffic_matrix_trace_iter_tms_t *iter = 
    malloc(sizeof(struct traffic_matrix_trace_iter_tms_t));

  iter->tms = tm;
  iter->tms_length = size - 1;

  iter->begin       = _ttit_begin;
  iter->end         = _ttit_end;
  iter->next        = _ttit_next;
  iter->get         = _ttit_get;
  iter->get_nocopy  = _ttit_get_nocopy;
  iter->free        = _ttit_free;
  iter->length      = _ttit_length;
  iter->go_to       = _ttit_go_to;

  return (struct traffic_matrix_trace_iter_t *)iter;
}
