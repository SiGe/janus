#include <stdio.h>
#include <stdlib.h>

#include "inih/ini.h"
#include "networks/jupiter.h"
#include "util/log.h"

#include "config.h"

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

static void jupiter_add_upgrade_group(char const *string, struct jupiter_sw_up_list_t *list) {
    int tot_read;
    char sw_type[256] = {0};
    int location, count, color;

    if (sscanf(string, "%[^-]-%d-%d-%d%n", sw_type, &location, &count, &color, &tot_read) <= 0) {
        panic("Bad format specifier for jupiter: %s", string);
    }

    if (tot_read < strlen(string)) {
        panic("Bad format specifier for switch-upgrade: %s", string);
    }

    list->size += 1;
    list->sw_list = realloc(list->sw_list, sizeof(struct jupiter_sw_up_list_t) * list->size);
    list->num_switches += count;

    struct jupiter_sw_up_t *up = &list->sw_list[list->size-1];
    up->count = count;
    up->location = location;
    up->color = color;

    if (strcmp(sw_type, "core") == 0) {
        up->type = CORE;
    } else if (strcmp(sw_type, "pod/agg") == 0) {
        up->type = AGG;
    } else {
        panic("Bad switch type specified: %s", sw_type);
    }
}

static uint32_t
jupiter_add_freedom_degree(char const *string,
        uint32_t **freedom) {
    int ndegree = 1;
    for (uint32_t i = 0; i < strlen(string); ++i) {
        ndegree += (string[i] == '-');
    }

    *freedom = malloc(sizeof(uint32_t) * ndegree);
    char *str = strdup(string);
    char const *ptr = strtok(str, "-");
    int degree = 0;

    while (ptr != 0) { 
        (*freedom)[degree++] = (uint32_t)atoi(ptr);
        ptr = strtok(0, "-");
    }

    free(str);
    return ndegree;
}

static int config_handler(void *data, 
        char const *section, char const *name,
        char const *value) {
    #define MATCH_SECTION(s) strcmp(section, s) == 0
    #define MATCH_NAME(n) strcmp(name, n) == 0
    #define MATCH(s, n) MATCH_SECTION(s) && MATCH_NAME(n)

    struct expr_t *expr = (struct expr_t *)data;

    if        (MATCH("", "traffic-test")) {
        expr->traffic_test = strdup(value);
    } else if (MATCH("", "traffic-training")) {
        expr->traffic_training = strdup(value);
    } else if (MATCH("", "risk-violation")) {
		expr->risk_violation = risk_violation_name_to_func(value);
    } else if (MATCH("", "risk-delay")) {
		expr->risk_delay = risk_delay_name_to_func(value);
    } else if (MATCH("", "network")) {
		expr->network = jupiter_string_to_network(value);
    } else if (MATCH_SECTION("upgrade")) {
        if (MATCH_NAME("switch-group")) {
            jupiter_add_upgrade_group(value, &expr->upgrade_list);
        } else if (MATCH_NAME("freedom")) {
            expr->upgrade_nfreedom = jupiter_add_freedom_degree(value, &expr->upgrade_freedom);
        } else {
            panic("Upgrading %s not supported.", name);
        }
    }
	
	return 1;
}

static void
_build_located_switch_group(struct expr_t *expr) {
    //TODO: Assume jupiter network for now.
    int nswitches = 0;
    for (uint32_t i = 0; i < expr->upgrade_list.size; ++i) {
        nswitches += expr->upgrade_list.sw_list[i].count;
    }

    struct jupiter_located_switch_t *sws = malloc(sizeof(struct jupiter_located_switch_t) * nswitches);
    uint32_t idx = 0;
    for (uint32_t i = 0; i < expr->upgrade_list.size; ++i) {
        struct jupiter_sw_up_t *up = &expr->upgrade_list.sw_list[i];
        for (uint32_t j = 0; j < up->count; ++j) {
            sws[idx].color = up->color;
            sws[idx].type = up->type;
            sws[idx].pod = up->location;
            if (up->type == CORE) {
                sws[idx].sid = jupiter_get_core(expr->network, j);
            } else if (up->type == AGG) {
                sws[idx].sid = jupiter_get_agg(expr->network, up->location, j);
            } else {
                panic("Unsupported type for located_switch.");
            }
            idx++;
        }
    }

    printf("Draining switches: ");
    for (uint32_t i =0; i < idx; ++i) {
      printf("%d, ", sws[i].sid);
    }
    printf("\n");

    expr->located_switches = sws;
}

void config_parse(char const *ini_file, struct expr_t *expr) {
    info("Parsing config %s", ini_file);
    if (ini_parse(ini_file, config_handler, expr) < 0) {
        panic("Couldn't load the ini file.");
    }
    _build_located_switch_group(expr);
}

