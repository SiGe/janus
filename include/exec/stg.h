#ifndef _EXEC_STG_H_
#define _EXEC_STG_H_

#include "exec.h"

/* TODO: OBSOLETE FOR NOW
 *
 * - Omid 03/23/2019
 */

struct stg_critical_path_t {
  struct jupiter_located_switch_t **sws;

  int    num_switches;
  int    idx;
  int    priority;
  bw_t   bandwidth;
};

struct stg_upgrade_plan_t {
  struct stg_critical_path_t *paths;
  unsigned num_paths;
};

struct exec_stg_t {
  struct exec_t;
  struct rvar_t          **steady_packet_loss;
  struct traffic_stats_t *pod_stats;
  struct traffic_stats_t *core_stats;
  uint32_t num_pods;

  struct stg_upgrade_plan_t *plan;

  unsigned *pods;
};

struct exec_t *exec_stg_create(void);

#endif // _EXEC_PUG_H_
