#ifndef _EXPERIMENT_H_
#define _EXPERIMENT_H_

#include <stdint.h>
#include "dataplane.h"
#include "plan.h"
#include "traffic.h"

struct network_t;
typedef uint32_t network_steps_t;
typedef uint32_t switch_id_t;

typedef int (*apply_mops_t) (struct network_t *, struct mop_t*);
typedef int (*step_t) (struct network_t *);
typedef int (*set_traffic_t) (struct network_t *, struct traffic_matrix_t const*);
typedef int (*get_traffic_t) (struct network_t *, struct traffic_matrix_t const**);
typedef int (*get_dataplane_t) (struct network_t *, struct dataplane_t *);

struct network_t {
  /* Apply a list of mops to the network */
  apply_mops_t            apply;

  /* Run a step of the network */
  step_t                  step;

  /* Set the traffic of the network (for that specific step) */
  set_traffic_t           set_traffic;

  /* Get the traffic of the network (for the last run of the network) */
  get_traffic_t           get_traffic;

  /* Get dataplane */
  get_dataplane_t         get_dataplane;

  /* Supported networking operations */
  void (*drain_switch)   (struct network_t *, switch_id_t);
  void (*undrain_switch) (struct network_t *, switch_id_t);

  struct traffic_matrix_t const *tm;
};

struct clone_t {
  /* Cloning function of a simulated network */
  struct simulated_network_t (*clone) (struct simulated_network_t *);

  /* Save the state of the network, so we can restore it later fast (useful for MCMC). */
  void (*save) (struct simulated_network_t *);

  /* Restore the state of the network (useful for MCMC). */
  void (*restore) (struct simulated_network_t *);
};

struct planner_t {
  struct simulated_network_t *sim_network;
  struct network_t           *phy_network;
};

#endif // _EXPERIMENT_H_
