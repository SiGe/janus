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
