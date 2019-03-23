#include <assert.h>
#include <dirent.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>

#include "algo/array.h"
#include "algo/maxmin.h"
#include "config.h"
#include "freelist.h"
#include "network.h"
#include "predictors/rotating_ewma.h"
#include "predictors/perfect.h"
#include "util/common.h"
#include "util/monte_carlo.h"

#include "exec.h"

struct _rvar_cache_builder_parallel {
  struct traffic_matrix_t **tms;
  uint32_t index;
  struct expr_t const *expr;
  struct freelist_repo_t *network_freelist;
  pthread_mutex_t *lock;
};

struct predictor_t *exec_perfect_cache_build_or_load(
    struct exec_t *exec, struct expr_t const *expr) {
  struct predictor_perfect_t *perfect = predictor_perfect_load(exec->trace);

  return (struct predictor_t *)perfect;
}

struct predictor_t *exec_ewma_cache_build_or_load(
    struct exec_t *exec, struct expr_t const *expr) {
  struct predictor_rotating_ewma_t *ewma = predictor_rotating_ewma_load(
      expr->cache.ewma_directory, EXEC_EWMA_PREFIX,
      expr->mop_duration + 1, exec->trace->num_indices, exec->trace);

  /* If we succeeded in loading the EWMA predictor */
  if (ewma) {
    return (struct predictor_t *)ewma;
  }

  /* Else create the EWMA predictor */
  if (!dir_exists(expr->cache.ewma_directory)) {
    info("Creating EWMA cache directory: %s", expr->cache.ewma_directory);
    dir_mk(expr->cache.ewma_directory);
  }

  char path[PATH_MAX] = {0};
  snprintf(path, PATH_MAX - 1, "%s"PATH_SEPARATOR"%s", expr->cache.ewma_directory, EXEC_EWMA_PREFIX);
  info("Building the EWMA cache files: %s", path);
  ewma = predictor_rotating_ewma_create(expr->ewma_coeff, expr->mop_duration + 1, path);
  ewma->build((struct predictor_t *)ewma, exec->trace_training);
  predictor_rotating_ewma_save((struct predictor_t *)ewma);

  return (struct predictor_t *)ewma;
}

struct array_t **
exec_rvar_cache_load_into_array(struct expr_t const *expr, unsigned *count) {
  char const *cache_dir = expr->cache.rvar_directory;

  DIR *dir = 0;
  struct dirent *ent = 0;
  unsigned nfiles = dir_num_files(cache_dir);
  if (nfiles == 0)
    return 0;

  struct array_t **ret = malloc(sizeof(struct array_t *) * nfiles);
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
      size_t nbytes = file_read(cache_file, &value);
      struct array_t *arr = array_deserialize(value, nbytes);
      ret[index] = arr;

      free(value);
      fclose(cache_file);
    }
    closedir (dir);
  } else {
    /* could not open directory */
    panic("Couldn't open the rvar_cache dir: %s", cache_dir);
    return 0;
  }

  *count = nfiles;

  return ret;
}

struct rvar_t **
exec_rvar_cache_load(struct expr_t const *expr, unsigned *count) {
  struct array_t **arr = exec_rvar_cache_load_into_array(expr, count);
  if (!arr) {
    panic("Could not load the rvar cache files: %s", expr->cache.rvar_directory);
    return 0;
  }

  unsigned ncount = *count;
  struct rvar_t **ret = malloc(sizeof(struct rvar_t *) * ncount);

  for (unsigned i = 0; i < ncount; ++i) {
    rvar_type_t *vals = 0;
    unsigned nvals = array_transfer_ownership(arr[i], (void**)&vals);
    free(arr[i]);
    ret[i] = rvar_sample_create_with_vals(vals, nvals);
  }

  free(arr);
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


static rvar_type_t _sim_network_for_mlu(void *data) {
  struct _rvar_cache_builder_parallel* builder = (struct _rvar_cache_builder_parallel*)data;
  struct traffic_matrix_t *tm = 0;

  // Get the next traffic matrix
  tm = builder->tms[builder->index];

  rvar_type_t mlu = 0;
  {
    // Simulate the network
    struct _network_dp_t *np = freelist_get(builder->network_freelist);
    np->net->set_traffic(np->net, tm);
    np->net->get_dataplane(np->net, &np->dp);

    maxmin(&np->dp);

    mlu = dataplane_mlu(&np->dp);
    freelist_return(builder->network_freelist, np);
  }

  // Count the violations
  return mlu;
}

static void
_exec_net_dp_create(
    struct exec_t *exec,
    struct expr_t const *expr) {

  unsigned nthreads = get_ncores() - 1;
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

rvar_type_t *
exec_simulate_ordered(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length) {
  pthread_mutex_t mut;
  if (pthread_mutex_init(&mut, 0) != 0)
    panic("Couldn't initiate the mutex: %p", &mut);

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

  rvar_type_t *vals = monte_carlo_parallel_ordered_rvar(
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
  return vals;
}

rvar_type_t *
exec_simulate_mlu(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct traffic_matrix_t **tms,
    uint32_t trace_length) {
  pthread_mutex_t mut;
  if (pthread_mutex_init(&mut, 0) != 0)
    panic("Couldn't initiate the mutex: %p", &mut);

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

  rvar_type_t *vals = monte_carlo_parallel_ordered_rvar(
      _sim_network_for_mlu, 
      data, trace_length,
      sizeof(struct _rvar_cache_builder_parallel), 0);

  // Free the dataplane resources used during simulation
  for (uint32_t i = 0; i < nthreads; ++i) {
    dataplane_free_resources(&networks[i]->dp);
  }

  free(networks);
  return vals;
}

struct rvar_t *
exec_simulate(
    struct exec_t *exec,
    struct expr_t const *expr,
    struct mop_t *mop,
    struct traffic_matrix_t **tms,
    uint32_t trace_length) {
  rvar_type_t *vals = exec_simulate_ordered(exec, expr, mop, tms, trace_length);
  return rvar_sample_create_with_vals(vals, trace_length);
}

risk_cost_t exec_plan_cost(
    struct exec_t *exec,
    struct expr_t const *expr, struct mop_t **mops,
    uint32_t nmops, trace_time_t start) {
  if (!exec->net_dp)
    _exec_net_dp_create(exec, expr);

  struct _network_dp_t *net_dp = freelist_get(exec->net_dp);
  struct network_t *net = net_dp->net;
  struct dataplane_t *dp = &net_dp->dp;
  risk_cost_t cost = 0;

  struct traffic_matrix_trace_t *trace = exec->trace;
  struct traffic_matrix_trace_iter_t *iter = trace->iter(trace);

  // This should match what pug and other planners do ...
  iter->go_to(iter, start + 1);

  struct traffic_matrix_t *tm = 0;
  uint32_t num_tors = expr->num_pods * expr->num_tors_per_pod;
  uint32_t num_tor_pairs = num_tors * num_tors;

  int violations = 0;
  for (uint32_t i = 0; i < nmops; ++i) {
    mops[i]->pre(mops[i], net);
    bw_t subplan_cost = 0;

    for (uint32_t step = 0; step < expr->mop_duration; ++step) {
      iter->get(iter, &tm);

      if (!tm)
        panic("Traffic matrix is nil.  Possibly reached the end of the trace: %d", step);

      /* Network traffic */
      net->set_traffic(net, tm);
      net->get_dataplane(net, dp);
      maxmin(dp);

      violations = dataplane_count_violations(dp, 0);
      subplan_cost += expr->risk_violation_cost->cost( expr->risk_violation_cost,
          ((rvar_type_t)violations/(rvar_type_t)(num_tor_pairs)));
      dataplane_free_resources(dp);

      iter->next(iter);
      traffic_matrix_free(tm);
    }

    /*
    char *out = mops[i]->explain(mops[i], expr->network);
    info("%s", out);
    free(out);
    */

    info("%d(th) subplan (%d switches) cost is: %f", 
        i, mops[i]->size(mops[i]), subplan_cost);
    cost += subplan_cost;

    mops[i]->post(mops[i], net);
  }

  freelist_return(exec->net_dp, net_dp);

  risk_cost_t time_cost = expr->criteria_time->cost(expr->criteria_time, nmops);
  return cost + time_cost;
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
    pods[i].out.min = INFINITY;
    pods[i].in.min = INFINITY;
    pods[i].pod_id = i;
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


static int _ts_cmp(void const *v1, void const *v2) {
  struct traffic_stats_t *t1 = (struct traffic_stats_t *)v1;
  struct traffic_stats_t *t2 = (struct traffic_stats_t *)v2;

  if       (t1->in.max < t2->in.max) return -1;
  else if  (t1->in.max > t2->in.max) return  1;

  return 0;
}

struct exec_critical_path_stats_t *exec_critical_path_analysis(
    struct exec_t const *exec, struct expr_t const *expr,
    struct traffic_matrix_trace_iter_t *iter,
    uint32_t iter_length) {

  struct traffic_stats_t *pod_stats = 0;
  struct traffic_stats_t *core_stats = 0;
  uint32_t num_pods = 0;

  /* Get traffic stats for the long-term planner */
  exec_traffic_stats(
      exec, expr, iter, iter_length,
      &pod_stats, &num_pods, &core_stats);

  qsort(pod_stats, num_pods, sizeof(struct traffic_stats_t), _ts_cmp);
  struct exec_critical_path_stats_t *plan = malloc(
      sizeof(struct exec_critical_path_stats_t));
  plan->num_paths = 0;
  uint32_t num_groups = num_pods + 1 /* core switches */;

  // This is the maximum size of the path
  plan->paths = malloc(sizeof(struct exec_critical_path_t) * num_groups);
  memset(plan->paths, 0, sizeof(struct exec_critical_path_t) * num_groups);
  struct exec_critical_path_t *paths = plan->paths;


  /* The rest of this section builds the critical path component for the
   * upgrade.  The way it works is quite simple.  Get the max bandwidth of each
   * pod and the core switches.  Get the number of switches that we are
   * upgrading in each pod/core.  The critical path would be the path with the
   * max bandwidth/#upgrades */

  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    uint32_t id = pod_stats[i].pod_id;
    paths[id].bandwidth = pod_stats[i].in.max;

    // Max number of switches to upgrade
    paths[id].sws = malloc(sizeof(
          struct jupiter_located_switch_t *) * expr->num_aggs_per_pod);
  }
  paths[num_groups - 1].bandwidth = core_stats->in.max;
  paths[num_groups - 1].sws = malloc(
      sizeof(struct jupiter_located_switch_t *) * expr->num_cores);

  for (uint32_t i = 0; i < expr->nlocated_switches; ++i) {
    struct jupiter_located_switch_t *sw = &expr->located_switches[i];
    paths[sw->pod].pod = sw->pod;
    paths[sw->pod].type = sw->type;
    if (sw->type == JST_AGG) {
      paths[sw->pod].sws[paths[sw->pod].num_switches++] = sw;
    } else if (sw->type == JST_CORE) {
      paths[num_groups - 1].sws[paths[num_groups - 1].num_switches++] = sw;
    } else {
      panic("Unsupported switch type: %d", sw->type);
    }
  }

  for (uint32_t i = 0; i < expr->num_pods; ++i) {
    uint32_t id = pod_stats[i].pod_id;
    paths[id].bandwidth = pod_stats[i].in.max;
  }
  paths[num_groups - 1].bandwidth = core_stats->in.max;
  plan->num_paths = num_groups;

  free(pod_stats);
  free(core_stats);

  return plan;
}

void exec_critical_path_analysis_update(
    struct exec_t const *exec, struct expr_t const *expr,
    struct traffic_matrix_t **tm, uint32_t ntms,
    struct exec_critical_path_stats_t *stats) {
  struct traffic_matrix_trace_iter_t *iter = 
    traffic_matrix_iter_from_tms(tm, ntms);

  struct traffic_stats_t *pod_stats = 0, *core_stats = 0;
  uint32_t num_pods = 0;
 
  // Get the traffic stats and update the paths
  exec_traffic_stats(exec, expr, iter, ntms, 
      &pod_stats, &num_pods, &core_stats);

  for (uint32_t i = 0; i < stats->num_paths; ++i) {
    if (stats->paths[i].type == JST_CORE) {
      stats->paths[i].cur_bandwidth = core_stats->in.max;
    } else if (stats->paths[i].type == JST_AGG) {
      stats->paths[i].cur_bandwidth = pod_stats[stats->paths[i].pod].in.max;
    }
  }
}

struct predictor_t *exec_predictor_create(struct exec_t *exec, struct expr_t const *expr, char const *value) {
  if        (strcmp(value, "ewma") == 0) {
    info("Loading %s predictor.", value);
    return exec_ewma_cache_build_or_load(exec, expr);
  } else if (strcmp(value, "perfect") == 0) {
    info("Loading %s predictor.", value);
    return exec_perfect_cache_build_or_load(exec, expr);
  }
  return 0;
}


