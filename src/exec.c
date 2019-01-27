#include <assert.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include "algo/maxmin.h"
#include "config.h"
#include "freelist.h"
#include "network.h"
#include "predictors/ewma.h"
#include "util/common.h"

#include "exec.h"

struct _rvar_cache_builder_parallel {
  struct traffic_matrix_t **tms;
  uint32_t index;
  struct expr_t *expr;
  struct freelist_repo_t *network_freelist;
  pthread_mutex_t *lock;
};

struct predictor_t *exec_ewma_cache_build_or_load(struct expr_t const *expr) {
  struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(1, expr->traffic_test);
  struct predictor_ewma_t *ewma = predictor_ewma_load(
      expr->cache.ewma_directory,
      EXEC_EWMA_PREFIX, expr->mop_duration + 1, trace->num_indices);

  if (ewma) {
    traffic_matrix_trace_free(trace);
    return (struct predictor_t *)ewma;
  }

  if (!dir_exists(expr->cache.ewma_directory)) {
    info("Creating EWMA cache directory.");
    dir_mk(expr->cache.ewma_directory);
  }

  info("Building the EWMA cache files.");
  char path[PATH_MAX] = {0};
  snprintf(path, PATH_MAX - 1, "%s"PATH_SEPARATOR"%s", expr->cache.ewma_directory, EXEC_EWMA_PREFIX);
  ewma = predictor_ewma_create(expr->ewma_coeff, expr->mop_duration + 1, path);
  ewma->build((struct predictor_t *)ewma, trace);
  predictor_ewma_save((struct predictor_t *)ewma);
  traffic_matrix_trace_free(trace);

  return (struct predictor_t *)ewma;
}

struct rvar_t **
exec_rvar_cache_load(struct expr_t const *expr, int *count) {
  char const *cache_dir = expr->cache.rvar_directory;

  DIR *dir = 0;
  struct dirent *ent = 0;
  int nfiles = dir_num_files(cache_dir);
  if (nfiles == 0)
    return 0;

  struct rvar_t **ret = malloc(sizeof(struct rvar_t *) * nfiles);
  info("Total number of cache files: %d", nfiles);

  if ((dir = opendir(cache_dir)) != NULL) {
    /* print all the files and directories within directory */
    while ((ent = readdir(dir)) != NULL) {
      char *ptr = 0;
      if ((ptr = strstr(ent->d_name, ".tsv")) == 0)
        continue;

      char path[PATH_MAX] = {0};
      snprintf(path, PATH_MAX - 1, "%s" PATH_SEPARATOR "%s", cache_dir, ent->d_name);
      FILE *cache_file = fopen(path, "r");

      *ptr = 0;
      int index = atoi(ent->d_name);
      char *value = 0;
      file_read(cache_file, &value);
      ret[index] = rvar_deserialize(value);
      free(value);
      fclose(cache_file);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    panic("Couldn't open the rvar_cache dir: %s", cache_dir);
    return 0;
  }

  return ret;
}

struct _network_dp_t {
  struct dataplane_t dp;
  struct network_t *net;
};

static rvar_type_t _sim_network_for_trace_parallel(void *data) {
  struct _rvar_cache_builder_parallel* builder = (struct _rvar_cache_builder_parallel*)data;
  struct traffic_matrix_t *tm = 0;

  // Get the next traffic matrix
  tm = builder->tms[builder->index];

  int violations = 0;
  {
    // Simulate the network
    struct _network_dp_t *np = freelist_get(builder->network_freelist);
    np->net->set_traffic(np->net, tm);
    np->net->get_dataplane(np->net, &np->dp);

    maxmin(&np->dp);

    violations = dataplane_count_violations(&np->dp, builder->expr->promised_throughput);
    freelist_return(builder->network_freelist, np);
  }

  // Count the violations
  rvar_type_t percentage = (rvar_type_t)violations/(rvar_type_t)(tm->num_pairs);

  return percentage;
}

static void
_exec_net_dp_create(
    struct exec_t *exec,
    struct expr_t *expr) {

  int nthreads = get_ncores() - 1;
  exec->net_dp = freelist_create(nthreads);
  struct _network_dp_t *networks = malloc(sizeof(struct _network_dp_t) * nthreads);

  for (uint32_t i = 0; i < nthreads; ++i) {
    networks[i].net = expr->clone_network(expr);
    memset(&networks[i].dp, 0, sizeof(struct dataplane_t));
    freelist_return(exec->net_dp, &networks[i]);
  }
}

static void __attribute__((unused))
_exec_net_dp_free(
    struct exec_t *exec,
    struct expr_t *expr) {
  uint32_t nthreads = exec->net_dp->size;
  for (uint32_t i = 0; i < nthreads; ++i) {
    struct _network_dp_t *network = freelist_get(exec->net_dp);
    network->net->free(network->net);
  }
}

struct rvar_t *
exec_simulate(
    struct exec_t *exec,
    struct expr_t *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length) {

  pthread_mutex_t mut;
  if (pthread_mutex_init(&mut, 0) != 0)
    panic("Couldn't initiate the mutex.");

  if (!exec->net_dp)
    _exec_net_dp_create(exec, expr);

  uint32_t nthreads = freelist_size(exec->net_dp);
  struct freelist_repo_t *repo = exec->net_dp;
  struct _network_dp_t **networks = malloc(sizeof(struct _network_dp_t *) * nthreads);

  /* Build a list of available networks */
  for (uint32_t i = 0; i < nthreads; ++i) { 
    networks[i] = freelist_get(repo);
  }

  for (uint32_t i = 0; i < nthreads; ++i) { 
    freelist_return(repo, networks[i]);
  }

  /* Apply the mop on the network */
  for (uint32_t j = 0; j < nthreads; ++j) {
    mop->pre(mop, networks[j]->net);
  }


  /* Fill out the data structure for parallel execution */
  struct _rvar_cache_builder_parallel *data = 
    malloc(sizeof(struct _rvar_cache_builder_parallel) * trace_length);

  for (uint32_t j = 0; j < trace_length; ++j ){
    data[j].lock = &mut;
    data[j].tms = tms;
    data[j].index = j;
    data[j].network_freelist = repo;
    data[j].expr = expr;
  }

  struct rvar_t *rvar = (struct rvar_t *)rvar_monte_carlo_parallel(
      _sim_network_for_trace_parallel, 
      data, trace_length,
      sizeof(struct _rvar_cache_builder_parallel), 0);

  for (uint32_t j = 0; j < nthreads; ++j) {
    mop->post(mop, networks[j]->net);
  }

  // Free the dataplane resources used during simulation
  for (uint32_t i = 0; i < nthreads; ++i) {
    dataplane_free_resources(&networks[i]->dp);
  }

  free(networks);
  return rvar;
}

risk_cost_t exec_plan_cost(
    struct exec_t *exec,
    struct expr_t *expr, struct mop_t **mops,
    uint32_t nmops, trace_time_t start) {
  if (!exec->net_dp)
    _exec_net_dp_create(exec, expr);

  struct _network_dp_t *net_dp = freelist_get(exec->net_dp);
  struct network_t *net = net_dp->net;
  struct dataplane_t *dp = &net_dp->dp;
  risk_cost_t cost = 0;

  struct traffic_matrix_trace_t *trace = exec->trace;
  struct traffic_matrix_trace_iter_t *iter = trace->iter(trace);
  iter->go_to(iter, start + expr->mop_duration);

  struct traffic_matrix_t *tm = 0;
  uint32_t num_tors = expr->num_pods * expr->num_tors_per_pod;
  uint32_t num_tor_pairs = num_tors * num_tors;

  int violations = 0;
  for (uint32_t i = 0; i < nmops; ++i) {
    mops[i]->pre(mops[i], net);
    bw_t subplan_cost = 0;

    for (uint32_t step = 0; step < expr->mop_duration; ++step) {
      iter->get(iter, &tm);

      /* Network traffic */
      net->set_traffic(net, tm);
      net->get_dataplane(net, dp);
      maxmin(dp);

      violations = dataplane_count_violations(dp, 0);
      subplan_cost += expr->risk_violation_cost->cost( expr->risk_violation_cost,
          ((rvar_type_t)violations/(rvar_type_t)(num_tor_pairs)));
      dataplane_free_resources(dp);

      iter->next(iter);
    }

    info("Actual cost if the %d(th) subplan is: %f", i, subplan_cost);
    cost += subplan_cost;

    mops[i]->post(mops[i], net);
  }

  freelist_return(exec->net_dp, net_dp);
  return cost;
}

#define STATS(x) { \
  (x).mean += (x).sum; \
  (x).max  = MAX((x).max, (x).sum); \
  (x).min  = MIN((x).min, (x).sum); \
  (x).sum   = 0; \
}

#define FIX_AVG(x, y) { \
  (x).mean /= (y); \
}\

void exec_traffic_stats(
    struct exec_t const *exec,
    struct expr_t const *expr,
    struct traffic_matrix_trace_iter_t *iter,
    uint32_t ntms,
    struct traffic_stats_t **ret_pod_stats,
    uint32_t *ret_npods,
    struct traffic_stats_t **ret_core_stats) {
  struct traffic_matrix_t *tm = 0;

  uint32_t tidx = 0;
  uint32_t num_tors = expr->num_pods * expr->num_tors_per_pod;

  struct traffic_stats_t *pods = malloc(sizeof(struct traffic_stats_t) * expr->num_pods);
  memset(pods, 0, sizeof(struct traffic_stats_t) * expr->num_pods);
  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    pods[i].in.min = INFINITY;
    pods[i].out.min= INFINITY;
    pods[i].id = i;
  }
  struct traffic_stats_t *spod = 0, *dpod = 0;
  struct traffic_stats_t *core = malloc(sizeof(struct traffic_stats_t));
  memset(core, 0, sizeof(struct traffic_stats_t));
  core->in.min = INFINITY;
  core->out.min = INFINITY;

  while (!iter->end(iter) && tidx < ntms) {
    iter->get(iter, &tm);
    iter->next(iter);

    for (uint32_t sp = 0; sp < expr->num_pods; ++sp) {
      spod = &pods[sp];
      for (uint32_t dp = 0; dp < expr->num_pods; ++dp) {
        dpod = &pods[dp];
        for (uint32_t st = 0; st < expr->num_tors_per_pod; ++st) {
          for (uint32_t dt = 0; dt < expr->num_tors_per_pod; ++dt) {
            uint32_t sid = sp * expr->num_tors_per_pod + st;
            uint32_t did = dp * expr->num_tors_per_pod + dt;
            uint32_t id = sid * num_tors + did;

            bw_t tr = tm->bws[id].bw;
            spod->out.sum += tr;
            dpod->in.sum += tr;

            if (spod != dpod) {
              core->out.sum += tr;
              core->in.sum += tr;
            }
          }
        }
      }
    }

    for (uint32_t i = 0; i < expr->num_pods; ++i) {
      STATS(pods[i].in);
      STATS(pods[i].out);
    }

    STATS(core->in);
    STATS(core->out);
    tidx++;
  }

  info("Read: %d", tidx);

  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    FIX_AVG(pods[i].in, tidx);
    FIX_AVG(pods[i].out, tidx);
  }
  FIX_AVG(core->out, tidx);
  FIX_AVG(core->in, tidx);

  assert(tm->num_pairs == num_tors * num_tors);

  *ret_pod_stats = pods;
  *ret_npods = expr->num_pods;
  *ret_core_stats = core;
}
