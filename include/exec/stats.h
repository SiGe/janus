#ifndef _EXEC_TRAFFIC_STATS_H_
#define _EXEC_TRAFFIC_STATS_H_
#include "exec.h"

/* Stats executor returns the stats for the pods and the core switches:
 * Stats is the max/min/and average (in/out) traffic over the interval.
 */
struct exec_t *exec_traffic_stats_create(void);

#endif  // _EXEC_TRAFFIC_STATS_H_
