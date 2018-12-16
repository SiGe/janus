#ifndef _TRAFFIC_H_
#define _TRAFFIC_H_

#include <stdint.h>

struct traffic_matrix_t {
  /* Bandwidths between the hosts */
  struct pair_bw_t *bws;

  /* Number of hosts */
  uint32_t num_pairs;
};


#endif // _TRAFFIC_H_

