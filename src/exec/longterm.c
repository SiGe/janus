#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#include "algo/maxmin.h"
#include "util/common.h"
#include "config.h"
#include "dataplane.h"
#include "network.h"
#include "freelist.h"
#include "plan.h"
#include "util/common.h"

#include "exec/longterm.h"

/* TODO: Merge this and use exec_simulate
 *
 * Omid - 1/25/2019
 * */
struct _rvar_cache_builder_parallel {
  struct traffic_matrix_trace_t *trace;
  uint32_t index;
  struct expr_t *expr;
  struct freelist_repo_t *network_freelist;
  pthread_mutex_t *lock;
};

struct _network_dp_t {
  struct dataplane_t dp;
  struct network_t *net;
};

static rvar_type_t _sim_network_for_trace_parallel(void *data) {
  struct _rvar_cache_builder_parallel* builder = (struct _rvar_cache_builder_parallel*)data;
  struct traffic_matrix_t *tm = 0;
  trace_time_t time = 0;

  {
    // Get the next traffic matrix
    pthread_mutex_lock(builder->lock);

    traffic_matrix_trace_get_nth_key(builder->trace, builder->index, &time);
    traffic_matrix_trace_get(builder->trace, time, &tm);
    pthread_mutex_unlock(builder->lock);
  }

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

  // And free the traffic matrix
  traffic_matrix_free(tm);

  return percentage;
}


static void _build_rvar_cache_parallel(struct expr_t *expr) {
  struct jupiter_switch_plan_enumerator_t *en = 
    jupiter_switch_plan_enumerator_create(
        expr->upgrade_list.num_switches,
        expr->located_switches,
        expr->upgrade_freedom,
        expr->upgrade_nfreedom);

  struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(400, expr->traffic_test);
  struct plan_iterator_t *iter = en->iter((struct plan_t *)en);
  int subplan_count = iter->subplan_count(iter);
  pthread_mutex_t mut;

  if (pthread_mutex_init(&mut, 0) != 0)
    panic("Couldn't initiate the mutex.");

  uint32_t trace_length = trace->num_indices;
  uint32_t nthreads = get_ncores() - 1;
  struct freelist_repo_t *repo = freelist_create(nthreads);
  struct _network_dp_t *networks = malloc(sizeof(struct _network_dp_t) * nthreads);
  char path[PATH_MAX] = {0};

  for (uint32_t i = 0; i < nthreads; ++i) {
    networks[i].net = expr->clone_network(expr);
    memset(&networks[i].dp, 0, sizeof(struct dataplane_t));
    freelist_return(repo, &networks[i]);
  }

  /* Get the range of subplans we are going through */
  int subplan_start = MIN(subplan_count-1, expr->cache.subplan_start);
  int subplan_end = MIN(subplan_count-1, expr->cache.subplan_end);
  if (subplan_start == subplan_end && subplan_end == 0) {
    subplan_end = subplan_count - 1;
  }

  for (int i = subplan_start; i <= subplan_end; ++i) {
    // Apply the mop on the network
    struct mop_t *mop = iter->mop_for(iter, i);
    struct _rvar_cache_builder_parallel *data = 
      malloc(sizeof(struct _rvar_cache_builder_parallel) * trace_length);

    for (uint32_t j = 0; j < nthreads; ++j) {
      mop->pre(mop, networks[j].net);
    }

    for (uint32_t j = 0; j < trace_length; ++j ){
      data[j].lock = &mut;
      data[j].trace = trace;
      data[j].index = j;
      data[j].network_freelist = repo;
      data[j].expr = expr;
    }

    struct rvar_t *rvar = (struct rvar_t *)rvar_monte_carlo_parallel(
        _sim_network_for_trace_parallel, 
        data, trace_length,
        sizeof(struct _rvar_cache_builder_parallel), 0);

    for (uint32_t j = 0; j < nthreads; ++j) {
      mop->post(mop, networks[j].net);
    }

    int ser_size = 0;
    char *value = rvar->serialize(rvar, &ser_size);
    sprintf(path, "%s"PATH_SEPARATOR"%05d.tsv", expr->cache.rvar_directory, i);
    FILE *rvar_file = fopen(path, "w+");

    /* TODO: Testing is a mess atm ... just verified this online and it works.
     * I should move this to the test suite. 
     * - Omid 1/23/2019
     */
    // struct rvar_t *rv = rvar_deserialize(value);
    // assert(rv->expected(rv) == rvar->expected(rv));
    
    fwrite(value, 1, ser_size, rvar_file);
    free(value);
    fclose(rvar_file);

    info("Generated rvar for %ith subplan (expected viol: %f)", i, rvar->expected(rvar));
    free(mop);
  }

  // Free the free list of networks
  for (uint32_t i = 0; i < nthreads; ++i) {
    struct _network_dp_t *np = freelist_get(repo);
    dataplane_free_resources(&np->dp);
  }
  free(networks);
  freelist_free(repo);

  traffic_matrix_trace_free(trace);
  info("Done generating the rvars");
}

static void
_exec_longterm_runner(struct exec_t *exec, struct expr_t *expr) {
  _build_rvar_cache_parallel(expr);
}

static void
_exec_longterm_validate(struct exec_t *exec, struct expr_t const *expr) {
  EXEC_VALIDATE_STRING_SET(expr, cache.rvar_directory);
  if (expr->cache.subplan_start == expr->cache.subplan_end && expr->cache.subplan_end != 0) {
    panic("Start and end duration are equal.");
  }

  // Create the directory if it doesn't exist
  info("Checking directory existance: %s", expr->cache.rvar_directory);
  if (!dir_exists(expr->cache.rvar_directory)) {
    info("%s does not exist. Creating it now!", expr->cache.rvar_directory);
    dir_mk(expr->cache.rvar_directory);
  }
}

struct exec_t *exec_longterm_create(void) {
  struct exec_t *exec = malloc(sizeof(struct exec_longterm_t));

  exec->validate = _exec_longterm_validate;
  exec->run = _exec_longterm_runner;

  return exec;
}


