#ifndef _CONFIG_H_
#define _CONFIG_H_

#include "plan.h"
#include "risk.h"

struct jupiter_sw_up_t {
    enum JUPITER_SWITCH_TYPE type;
    int location; int count; int color;
};

struct jupiter_sw_up_list_t {
    struct jupiter_sw_up_t *sw_list;
    uint32_t size;
    uint32_t num_switches;
};

struct expr_longterm_t {
    uint32_t start, end;
    char const *rvar_directory;
};

struct expr_execution_t {
    uint32_t *at; // Simulation points
    uint32_t nat;
};

enum EXPR_ACTION {
    BUILD_LONGTERM, // Build the long-term cache files

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

    risk_func_t *risk_violation;
    risk_func_t *risk_delay;

    struct network_t *network;
    
    //TODO: Change this from Jupiter to arbitrary topology later on ...
    struct jupiter_sw_up_list_t upgrade_list;
    struct jupiter_located_switch_t *located_switches;

    // Freedom degree for the upgrades
    uint32_t *upgrade_freedom;
    uint32_t upgrade_nfreedom;

    // Function to clone the experiment network---used for monte-carlo
    // simulations later on.
    struct network_t * (*clone_network)(struct expr_t *);

    // Simulation points
    uint32_t *at;
    uint32_t nat;

    // Long-term cache files
    struct expr_longterm_t longterm;

    // Execution intervals, etc.
    struct expr_execution_t exec;

    // Experiment action
    enum EXPR_ACTION action;
};

void config_parse(char const *ini_file, struct expr_t *expr, int argc, char * const* argv);

#endif // _CONFIG_H_
