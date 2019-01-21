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

struct expr_t {
    char *traffic_test;
    char *traffic_training;

	risk_func_t *risk_violation;
	risk_func_t *risk_delay;

    struct network_t *network;
    
    //TODO: Change this from Jupiter to arbitrary topology later on ...
    struct jupiter_sw_up_list_t upgrade_list;
    struct jupiter_located_switch_t *located_switches;

    // Freedom degree for the upgrades
    uint32_t *upgrade_freedom;
    uint32_t upgrade_nfreedom;
};

void config_parse(char const *ini_file, struct expr_t *expr);

#endif // _CONFIG_H_
