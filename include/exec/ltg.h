#ifndef _EXEC_LTG_H_
#define _EXEC_LTG_H_

#include "exec.h"

/*
 * LTG (or as we call it in the paper MRC/maximize residual capacity planner),
 * is a planner that maximizes the amount of capacity that is left in the
 * network in each step.  It is hard to build this to optimally build this for
 * every case (it may be possible when the topology is super large?) but in
 * small topologies it's rather difficult cause you have more plan variety.
 *
 * In large topologies taking down a single switch has very little impact,
 * whereas in small topologies can cause a havoc in part of the network.
 *
 * TODO: Either way, LTG just takes down 1/n th of the switches in each block of the
 * topology during each stage.  This maximizes the residual capcaity for JUPITER
 * and JUPITER only.  I have not verified nor tested this for other topologies,
 * and I'd leave this part to future work.
 */
struct exec_ltg_t {
  struct exec_t;                      /* LTG is an executor */
  struct exec_critical_path_stats_t *plan;
};

struct exec_t *exec_ltg_create(void);

#endif // _EXEC_PUG_H_
