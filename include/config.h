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

  /* Predictor directories */
  char const *ewma_directory;
  char const *perfect_directory;
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
    RUN_PUG_LONG,  
    RUN_PUG_LOOKBACK,
    RUN_STG,
    RUN_LTG,
    RUN_CAP,

    // Unknown ...
    RUN_UNKNOWN,
};

enum EXPR_VERBOSE {
  VERBOSE_NONE = 0,
  VERBOSE_SUBPLANS = 3,
  VERBOSE_SHOW_ME_PLAN_RISK = 4,
  VERBOSE_SHOW_ME_EVERYTHING = 5,
};

struct scenario_t {
  uint32_t time_begin, time_end, time_step;
};

struct expr_t {
    char *traffic_test;
    char *traffic_training;
    char *network_string;

    struct network_t *network;
    trace_time_t mop_duration;

    // Predictor stuff
    float ewma_coeff;
    char *predictor_string;

    // Min throughput promised to the user
    bw_t promised_throughput;
    
    //TODO: Change this from Jupiter to arbitrary topology later on ...
    struct jupiter_sw_up_list_t upgrade_list;
    struct jupiter_located_switch_t *located_switches;
    int nlocated_switches;

    // Runtime scenario
    struct scenario_t scenario;

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

    // Verbosity
    enum EXPR_VERBOSE verbose;
    int explain;

    // Pug related configuration
    trace_time_t pug_backtrack_traffic_count;
    int          pug_is_backtrack;
};

void config_parse(char const *ini_file, struct expr_t *expr, int argc, char * const * argv);

#endif // _CONFIG_H_
