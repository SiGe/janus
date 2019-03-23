#include "util/common.h"
#include "util/log.h"
#include "traffic.h"

#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "khash.h"
#define NET_MAX 64

KHASH_MAP_INIT_STR(net_entity, char *)

void usage(char const *arg) {
  warn("\n  usage: %s [TM PATH] [OUTPUT]\n\n"
       "  Accept traffic matrix files from the interpolate.py output\n"
       "  And outputs a compressed format usable by the rest of the\n"
       "  toolkit.", arg
      );
  exit(1);
}

void insert_tor_pod(khash_t(net_entity) *table, char const *tor, char const *pod) {
    khiter_t iter = kh_get(net_entity, table, tor);
    if (iter != kh_end(table)) return;

    int absent = 0;
    iter = kh_put(net_entity, table, tor, &absent);
    if (absent) kh_key(table, iter) = strdup(tor);
    kh_value(table, iter) = strdup(pod);
}

uint32_t load_keys(char const *fdir) {
  char path[PATH_MAX] = {0};
  (void) strncat(path, fdir, PATH_MAX - 1);
  (void) strncat(path, "/key.tsv", PATH_MAX - 1);

  FILE *keys = fopen(path, "r");
  if (!keys)
    panic("Couldn't find the file %s", path);

  char tor1[NET_MAX], tor2[NET_MAX], pod1[NET_MAX], pod2[NET_MAX];
  khash_t(net_entity) *h = kh_init(net_entity);

  info_txt("Loading keys.");
  while (!feof(keys)) {
    fscanf(keys, "%s\t%s\t%s\t%s\t0\t0 ", tor1, tor2, pod1, pod2);
    insert_tor_pod(h, tor1, pod1);
    insert_tor_pod(h, tor2, pod2);
  }

  uint32_t tor_count = kh_size(h);
  for (khiter_t k = kh_begin(h); k != kh_end(h); ++k) {
    if (!kh_exist(h, k)) continue;

    char *val = kh_value(h, k);
    char const* key = kh_key(h, k);
    // TODO: Save file ...
    //info("Tor: %s, Pod: %s", key, val);

    kh_del(net_entity, h, k);
    free(val);
    free((char*) key);
  }
  info_txt("Finished loading keys.");

  kh_destroy(net_entity, h);
  return tor_count;
}

// https://stackoverflow.com/questions/12489/how-do-you-get-a-directory-listing-in-c
void for_file_in_dir(
    char const *dir,
    void (*exec)(char const *, void *),
    void *obj) {
  DIR *dp = 0;
  struct dirent *ep = 0;
  dp = opendir (dir);

  if (dp != NULL) {
    while ((ep = readdir (dp)) != 0) {
      exec(ep->d_name, obj);
    }

    (void) closedir (dp);
  }
  else
    panic("Couldn't open the directory: %s", dir);
}

void print_name(char const *name, void *nil) {
  info("%s", name);
  (void)(nil);
}

struct traffic_matrix_t *file_to_tm(
    char const *name, char const *dir, 
    uint32_t tor_count) {
  char fname[PATH_MAX+1]= {0};
  (void) strncat(fname, dir, PATH_MAX);
  (void) strncat(fname, name, PATH_MAX);
  
  FILE *file = fopen(fname, "r");
  bw_t bw = 0;
  size_t size = sizeof(struct traffic_matrix_t) +
    sizeof(struct pair_bw_t) * tor_count * tor_count;
  struct traffic_matrix_t *tm = malloc(size);
  struct pair_bw_t *bws = tm->bws;

  int src = 0, dst  = 0;
  uint64_t iterator   = 0;
  float max_traffic = 0;

  fseek(file, 0, SEEK_SET);
  while (!feof(file)) {
    if (src == dst) {
      bws->bw = 0;
      bws++;
      dst += 1;
      iterator++;
    }

    fscanf(file, "%f ", &bw);
    bws->bw = bw;
    bws++;

    max_traffic = (max_traffic < bw) ? bw : max_traffic;

    dst += 1;

    if (dst == tor_count) {
      dst = 0;
      src += 1;
    }

    iterator++;
  }

  /* TODO: Not sure what was wrong with the old code that requires me to insert
   * this line ... */
  bws->bw = 0;
  iterator++;

  tm->num_pairs = iterator;
  assert(iterator == tor_count * tor_count);
  fclose(file);

  return tm;
}

struct _trace_metadata {
  struct traffic_matrix_trace_t *trace;
  char const *dir;
  uint32_t tor_count;
};

void add_tm_to_trace(char const *name, void *_metadata) {
  struct _trace_metadata *metadata = 
    (struct _trace_metadata *)_metadata;
  char *tokens = strdup(name);

  if (name[0] != '0')
    return;

  char *split = strtok(tokens, ".");
  int key = atoi(split);
  struct traffic_matrix_t *tm =
    file_to_tm(name, metadata->dir, metadata->tor_count); 

  info("Serialized traffic matrix @key: %d", key);
  traffic_matrix_trace_add(
      metadata->trace, tm, key);

  free(tm);
  free(tokens);
}

void load_traffic(char const *fdir, const char *output, uint32_t tor_count) {
  struct traffic_matrix_trace_t *trace = 
    traffic_matrix_trace_create(50, 100, output);

  struct _trace_metadata metadata = {
    .trace =  trace,
    .dir = fdir,
    .tor_count = tor_count,
  };

  for_file_in_dir(
      fdir, add_tm_to_trace, (void *)&metadata);

  traffic_matrix_trace_save(trace);
  traffic_matrix_trace_free(trace);
}

int main(int argc, char **argv) {
  if (argc != 3)
    usage(argv[0]);

  char const *fdir   = argv[1];
  char const *output = argv[2];
  (void)(output);

  uint32_t tor_count = load_keys(fdir);
  load_traffic(fdir, output, tor_count);

  return 0;
}
