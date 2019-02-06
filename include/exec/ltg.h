#ifndef _EXEC_LTG_H_
#define _EXEC_LTG_H_

#include "exec.h"

struct exec_ltg_t {
  struct exec_t;
  struct rvar_t          **steady_packet_loss;
  struct traffic_stats_t *pod_stats;
  struct traffic_stats_t *core_stats;
  uint32_t num_pods;

  struct exec_critical_path_stats_t *plan;

  int *pods;
};

struct exec_t *exec_ltg_create(void);

#endif // _EXEC_PUG_H_
