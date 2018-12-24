#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include "dataplane.h"
#include <stdint.h>
#include <stdio.h>

typedef uint64_t trace_time_t;
typedef uint16_t host_id_t;


struct //__attribute__((packed))
pair_bw_t {
  /* Source id and destination id */
  host_id_t sid, did;

  /* Bandwidth used between the pairs */
  bw_t bw;
};

struct traffic_matrix_t {
  /* Number of hosts */
  uint32_t num_pairs;

  void (*save) (struct traffic_matrix_t *, FILE *);

  /* Bandwidths between the hosts */
  struct pair_bw_t bws[];
};

struct traffic_matrix_repository_t {
};

struct traffic_matrix_trace_index_t {
  uint64_t                seek;
  trace_time_t            time;
  uint64_t                size;
};

struct traffic_matrix_trace_cache_t {
  trace_time_t time;
  struct traffic_matrix_t *tm;
  uint8_t is_set;
};

struct traffic_matrix_trace_t {
  struct traffic_matrix_trace_index_t *indices;
  struct traffic_matrix_trace_cache_t *caches;

  uint64_t                             num_indices, cap_indices;
  uint16_t                             num_caches;
  uint64_t                             largest_seek;

  // Index and data files
  FILE                                *fdata, *findex;

  // Whether the indices are optimized (as in sorted) or not.
  uint8_t _optimized;
};

void traffic_matrix_free(struct traffic_matrix_t *);
void traffic_matrix_save(struct traffic_matrix_t *, FILE *);
struct traffic_matrix_t * traffic_matrix_load(FILE *);

void traffic_matrix_trace_save(struct traffic_matrix_trace_t *);

// Append only data-structure for working with traffic matrices
void traffic_matrix_trace_add(
    struct traffic_matrix_trace_t *,
    struct traffic_matrix_t *,
    trace_time_t key);

void traffic_matrix_trace_get(
    struct traffic_matrix_trace_t *,
    trace_time_t key,
    struct traffic_matrix_t **);

struct traffic_matrix_trace_t *traffic_matrix_trace_create(
    uint16_t, uint16_t, const char *);

struct traffic_matrix_trace_t *traffic_matrix_trace_load(
    uint16_t, const char *);

void traffic_matrix_trace_save(struct traffic_matrix_trace_t *);
void traffic_matrix_trace_free(struct traffic_matrix_trace_t *t);
void traffic_matrix_trace_print_index(struct traffic_matrix_trace_t *t);

#endif // _TRAFFIC_H_

