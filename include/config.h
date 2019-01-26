#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "plan.h"
#include "plan/jupiter.h"
#include "risk.h"
#include "traffic.h"

struct jupiter_sw_up_t {
    enum JUPITER_SWITCH_TYPE type;
    int location; int count; int color;
};

struct jupiter_sw_up_list_t {
    struct jupiter_sw_up_t *sw_list;
    uint32_t size;
    uint32_t num_switches;
};

struct expr_cache_t {
  /* Cache generation start to end */
  uint32_t subplan_start, subplan_end;

  /* Rvar directory */
  char const *rvar_directory;

  /* EWMA directory */
  char const *ewma_directory;
};

struct expr_execution_t {
    uint32_t *at; // Simulation points
    uint32_t nat;
};

enum EXPR_ACTION {
    BUILD_LONGTERM, // Build the long-term cache files
    TRAFFIC_STATS,  // Returns the traffic stats for the pods

    // Different simulators
    RUN_PUG,  
    RUN_STG,
    RUN_LTG,
    RUN_CAP,

    // Unknown ...
    RUN_UNKNOWN,
};

struct expr_t {
    char *traffic_test;
    char *traffic_training;
    char *network_string;

    struct network_t *network;
    trace_time_t mop_duration;

    float ewma_coeff;

    // Min throughput promised to the user
    bw_t promised_throughput;
    
    //TODO: Change this from Jupiter to arbitrary topology later on ...
    struct jupiter_sw_up_list_t upgrade_list;
    struct jupiter_located_switch_t *located_switches;
    int nlocated_switches;

    //TODO: Jupiter specific config ... should change later.
    uint32_t num_cores;
    uint32_t num_pods;
    uint32_t num_tors_per_pod;
    uint32_t num_aggs_per_pod;

    // Freedom degree for the upgrades
    uint32_t *upgrade_freedom;
    uint32_t upgrade_nfreedom;

    // Function to clone the experiment network---used for monte-carlo
    // simulations later on.
    struct network_t * (*clone_network)(struct expr_t *);

    // Simulation points
    uint32_t *at;
    uint32_t nat;

    // Execution cache files
    struct expr_cache_t cache;

    // Execution intervals, etc.
    struct expr_execution_t exec;

    // Experiment action
    enum EXPR_ACTION action;

    // Planning criteria
    struct criteria_time_t *criteria_time;
    struct risk_cost_func_t *risk_violation_cost;
    criteria_length_t criteria_plan_length;
};

void config_parse(char const *ini_file, struct expr_t *expr, int argc, char * const* argv);

#endif // _CONFIG_H_
