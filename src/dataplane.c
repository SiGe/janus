#include <math.h>
#include <stdlib.h>

#include "dataplane.h"

#define SAFE_FREE(p) {\
  if (p) { \
    free((p));\
  }\
  (p) = 0;\
}

void dataplane_init(struct dataplane_t *plane) {
  if (plane->links) {
    struct link_t *link = plane->links;
    for (uint32_t i = 0; i < plane->num_links; ++i) {
      SAFE_FREE(link->flows);
      link++;
    }
  }
  SAFE_FREE(plane->flows);
  SAFE_FREE(plane->links);
  SAFE_FREE(plane->routing);
  plane->smallest_flow = 0;
  plane->smallest_link = 0;
  plane->num_links = 0;
  plane->num_flows = 0;
}

void dataplane_free_resources(struct dataplane_t *plane) {
  if (plane->links) {
    struct link_t *link = plane->links;
    for (uint32_t i = 0; i < plane->num_links; ++i) {
      SAFE_FREE(link->flows);
      link++;
    }
  }
  SAFE_FREE(plane->flows);
  SAFE_FREE(plane->links);
  SAFE_FREE(plane->routing);
  plane->smallest_flow = 0;
  plane->smallest_link = 0;
  plane->num_links = 0;
  plane->num_flows = 0;
}

int dataplane_count_violations(struct dataplane_t const *dp, float max_bandwidth) {
  if (max_bandwidth == 0)
    max_bandwidth = INFINITY;

  int violations = 0;
  for (int flow_id = 0; flow_id < dp->num_flows; ++flow_id) {
    if (dp->flows[flow_id].bw < dp->flows[flow_id].demand && dp->flows[flow_id].bw < max_bandwidth) {
      violations +=1 ;
    }
  }

  return violations;
}
