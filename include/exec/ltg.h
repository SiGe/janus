#ifndef _EXEC_LTG_H_
#define _EXEC_LTG_H_
#include "exec.h"

// So the idea of LTG is to find the critical path for the long-term traffic
// and spread the upgrades over the long-term traffic.
void critical_path_plan(
    struct traffic_matrix_t **tms,
    int *plans) {
}



struct exec_ltg_t {
  struct exec_t;

  struct plan_repo_t     *plans;
  struct plan_t          *planner;
  struct plan_iterator_t *iter;

  struct predictor_t *pred;
  struct rvar_t      **steady_packet_loss;

  int shortterm_samples;
};

struct plan_repo_t {
  int *plans;
  int plan_count;
  int initial_plan_count;
  int max_plan_size;
  int plan_size_in_bytes;
  int cap;

  int _cur_index;
  int _subplan_count;
};

struct exec_pug_t {
  struct exec_t;

  struct plan_repo_t     *plans;
  struct plan_t          *planner;
  struct plan_iterator_t *iter;

  struct predictor_t *pred;
  struct rvar_t      **steady_packet_loss;

  int shortterm_samples;
};

struct exec_t *exec_pug_create(void);

#endif // _EXEC_PUG_H_
