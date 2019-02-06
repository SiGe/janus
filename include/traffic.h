#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include "dataplane.h"
#include <stdint.h>
#include <stdio.h>

typedef int64_t trace_time_t;
typedef uint16_t host_id_t;

struct //__attribute__((packed))
pair_bw_t {
  /* Unfortunately this doesn't work as well as I wanted because the traffic is
   * not as sparse as I thought it is.  Within interval more than 40\% of ToR
   * pairs have traffic ... which means there is not much benefit to this
   * approach vs. a dense matrix. */
  /* Source id and destination id */
  // host_id_t sid, did;

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

// Free the TM memory
void traffic_matrix_free(struct traffic_matrix_t *);

// Save the TM in the passed file.  The save starts to write wherever the cursor
// of the file is pointing to and onwards.  The way that we are saving the TM
// right now does not enforce a BIG/LITTLE endian in the files, so the traffic
// files are not portable across platforms that differ in endiness.
void traffic_matrix_save(struct traffic_matrix_t *, FILE *);

// Load a TM from the passed file.
struct traffic_matrix_t * traffic_matrix_load(FILE *);

// Index data structure for the trace
struct traffic_matrix_trace_index_t {
  // Location of the TM in the .data file
  uint64_t                seek;
  // Time associated with this TM
  trace_time_t            time;
  // Size of the TM, i.e., how much should we read :)
  uint64_t                size;
};

/* Cache structure for the trace */
struct traffic_matrix_trace_cache_t {
  trace_time_t time;
  struct traffic_matrix_t *tm;
  uint8_t is_set;
};

/* Adds two traffic matrices and outputs a new one */
struct traffic_matrix_t *traffic_matrix_add(
    struct traffic_matrix_t const *,
    struct traffic_matrix_t const *);

/* Multiplies all entries in a traffic matrix by a constant valueand outputs a
 * new one */
struct traffic_matrix_t *traffic_matrix_multiply(
    bw_t, struct traffic_matrix_t const *);


struct traffic_matrix_trace_iter_t {
    struct traffic_matrix_trace_t *trace;
    uint32_t state;
    uint32_t _begin, _end;

    void (*begin)(struct traffic_matrix_trace_iter_t *);
    void (*go_to)(struct traffic_matrix_trace_iter_t *, trace_time_t);
    int  (*length)(struct traffic_matrix_trace_iter_t *);
    int  (*next)(struct traffic_matrix_trace_iter_t *);
    int  (*end)(struct traffic_matrix_trace_iter_t *);
    void (*get)(struct traffic_matrix_trace_iter_t *, struct traffic_matrix_t **);
    void (*free)(struct traffic_matrix_trace_iter_t *);

    struct traffic_matrix_t* (*get_nocopy)(struct traffic_matrix_trace_iter_t *);
};

struct traffic_matrix_trace_iter_tms_t {
  struct traffic_matrix_trace_iter_t;
  struct traffic_matrix_t **tms;
  uint32_t tms_length;
};

// Append only data-structure for working with traffic matrix traces
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

  struct traffic_matrix_trace_iter_t * (*iter)(struct traffic_matrix_trace_t *);
};

// Save the trace to the two file pointers opened inside of it
void traffic_matrix_trace_save(struct traffic_matrix_trace_t *);

// Add a new TM to the trace.  It's append only (but that doesn't mean that the
// keys should be strictly increasing) so the data is written to the end of the
// .data file whereas the index is updated to keep track of it.
void traffic_matrix_trace_add(
    struct traffic_matrix_trace_t *,
    struct traffic_matrix_t *,
    trace_time_t key);

// Returns a TM associated with a key
void traffic_matrix_trace_get(
    struct traffic_matrix_trace_t *,
    trace_time_t key,
    struct traffic_matrix_t **);

int traffic_matrix_trace_get_nth_key(
    struct traffic_matrix_trace_t *,
    uint32_t, trace_time_t *);

// Creates a traffic matrix trace with the specified number of cache_slots and
// initiailized to have a specific number of indices.  The string passed is the
// prefix for the .index and .data files.  Number of elements in the cache is
// always rounded up to the nearest power of two
struct traffic_matrix_trace_t *traffic_matrix_trace_create(
    uint16_t, uint16_t, const char *);

// Load a trace (with the specified number of cache_slots) from the specified
// prefix file.
struct traffic_matrix_trace_t *traffic_matrix_trace_load(
    uint16_t, const char *);

// Save the trace to the file.  This only means writing the index to the .index
// file as the data is updated as the files are getting added (to avoid
// polluting the memory).
void traffic_matrix_trace_save(struct traffic_matrix_trace_t *);

// Free the trace by releasing all the set cache slots, closing the file
// pointers, and freeing the cache and indices data structures
void traffic_matrix_trace_free(struct traffic_matrix_trace_t *t);

// DEBUG function for printing out the index of a trace data structure
void traffic_matrix_trace_print_index(struct traffic_matrix_trace_t *t);

void traffic_matrix_trace_for_each(struct traffic_matrix_trace_t *t,
    int (*exec)(struct traffic_matrix_t *, trace_time_t, void*), void*);

// Outputs a new traffic matrix where all entries are zero
struct traffic_matrix_t *traffic_matrix_zero(pair_id_t);

struct traffic_matrix_trace_iter_t *traffic_matrix_iter_from_tms(
    struct traffic_matrix_t **tm, uint32_t size);

void trace_iterator_set_range(struct traffic_matrix_trace_iter_t *iter, 
    uint32_t begin, uint32_t end);

#endif // _TRAFFIC_H_

