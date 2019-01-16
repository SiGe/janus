#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "algo/maxmin.h"
#include "inih/ini.h"
#include "util/log.h"
#include "network.h"
#include "networks/jupiter.h"
#include "risk.h"

const char *usage_message = ""
  "usage: %s <experiment-setting ini file>\n";

void usage(const char *fname) {
  printf(usage_message, fname);
  exit(EXIT_FAILURE);
}

struct expr_t {
    char *traffic_test;
    char *traffic_training;

	risk_func_t *risk_violation;
	risk_func_t *risk_delay;

    struct network_t *network;

    int testing;
};

static risk_func_t *risk_violation_name_to_func(char const *name) {
	return 0;
}

static risk_func_t *risk_delay_name_to_func(char const *name) {
	return 0;
}

static struct network_t *jupiter_string_to_network(char const *string) {
    info("Parsing jupiter config: %s", string);
    uint32_t core, pod, app, tpp; bw_t bw;
    int tot_read;

    if (sscanf(string, "jupiter-%d-%d-%d-%d-%f%n", &core, &pod, &app, &tpp, &bw, &tot_read) <= 0) {
        panic("Bad format specifier for jupiter: %s", string);
    }

    if (tot_read < strlen(string)) {
        panic("Bad format specifier for jupiter: %s", string);
    }

    info("Creating a jupiter network with: %d, %d, %d, %d, %f", core, pod, app, tpp, bw);

    return (struct network_t *)jupiter_network_create(core, pod, app, tpp, bw);
}

static int config_handler(void *data, 
        char const *section, char const *name,
        char const *value) {
    #define MATCH(s, n) strcmp(section, s) == 0 && strcmp(name, n) == 0

    struct expr_t *expr = (struct expr_t *)data;
    if        (MATCH("", "traffic-test")) {
        expr->traffic_test = strdup(value);
    } else if (MATCH("", "traffic-training")) {
        expr->traffic_training = strdup(value);
    } else if (MATCH("", "risk-violation")) {
		expr->risk_violation = risk_violation_name_to_func(value);
    } else if (MATCH("", "risk-delay")) {
		expr->risk_delay= risk_delay_name_to_func(value);
    } else if (MATCH("", "network")) {
		expr->network = jupiter_string_to_network(value);
    }
	
	return 1;
}

void config_parse(char const *ini_file, struct expr_t *expr) {
    info("Parsing config %s", ini_file);
    if (ini_parse(ini_file, config_handler, expr) < 0) {
        panic("Couldn't load the ini file.");
    }
}

void test_settings(struct expr_t *expr) {
    struct dataplane_t dp = {0};
    struct traffic_matrix_trace_t *trace = traffic_matrix_trace_load(50, expr->traffic_test);
    struct traffic_matrix_trace_iter_t *iter = trace->iter(trace);
    uint32_t tm_idx = 1;

    float  mlu = 0;
    for (iter->begin(iter); !iter->end(iter); iter->next(iter)) {
        struct traffic_matrix_t *tm = 0;
        iter->get(iter, &tm);
        expr->network->set_traffic(expr->network, tm);
        expr->network->get_dataplane(expr->network, &dp);
        maxmin(&dp);

        int violations = 0;
        for (int flow_id = 0; flow_id < dp.num_flows; ++flow_id) {
            if (dp.flows[flow_id].bw < dp.flows[flow_id].demand) {
                violations +=1 ;
            }
        }

        bw_t cur_mlu = 0;
        for (int link_id = 0; link_id < dp.num_links; ++link_id) {
            struct link_t *link = &dp.links[link_id];
            bw_t link_util = link->used / link->capacity;
            if (cur_mlu < link_util)
                cur_mlu = link_util;
        }

        if (mlu < cur_mlu)
            mlu = cur_mlu;

        if (violations != 0)
            info("[TM %d] Number of violations: %d", tm_idx, violations);
        tm_idx ++;
    }
    info("Maximum network MLU is %f", mlu);

    iter->free(iter);
    traffic_matrix_trace_free(trace);
}

int main(int argc, char **argv) {
  if (argc < 2) {
    usage(argv[0]);
  }

  struct expr_t expr = {0};
  config_parse(argv[1], &expr);

  test_settings(&expr);

  return EXIT_SUCCESS;
}
