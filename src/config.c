#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/common.h"
#include "inih/ini.h"
#include "networks/jupiter.h"
#include "util/log.h"

#include "config.h"

risk_cost_t _rvc_cost(struct rvar_t const *rvar) {
  return rvar->percentile(rvar, 0.99); //expected(rvar);
}

static risk_func_t risk_violation_name_to_func(char const *name) {
  return _rvc_cost;
}

static int _cutoff_at(struct criteria_time_t *ct, uint32_t length) {
  return ct->steps >= length;
}

static struct criteria_time_t *risk_delay_name_to_func(char const *name) {
  char *ptr = strstr(name, "cutoff-at-");

  if (ptr == 0)
    panic("Unsupported criteria_time function: %s", name);

  ptr += strlen("cutoff-at-");
  struct criteria_time_t *ret = malloc(sizeof(struct criteria_time_t));
  ret->acceptable = _cutoff_at;
  ret->steps = atoi(ptr);

  return ret;
}

static int _cl_maximize(int a1, int a2) {
  if (a1 > a2) { return 1; }
  else if (a1 == a2) { return 0; }
  else if (a1 < a2) { return -1; }
  return -1;
}

static int _cl_minimize (int a1, int a2) {
  if (a1 > a2) { return -1; }
  else if (a1 == a2) { return 0; }
  else if (a1 < a2) { return 1; }
  return -1;
}

static criteria_length_t risk_length_name_to_func(char const *name) {
  if (strcmp(name, "maximize") == 0) {
    return _cl_maximize;
  } else if (strcmp(name, "minimize") == 0) {
    return _cl_minimize;
  }
  panic("Unsupported length criteria: %s", name);
  return 0;
}

static struct network_t *jupiter_string_to_network(struct expr_t *expr, char const *string) {
  uint32_t core, pod, app, tpp; bw_t bw;
  int tot_read;

  if (sscanf(string, "jupiter-%d-%d-%d-%d-%f%n", &core, &pod, &app, &tpp, &bw, &tot_read) <= 0) {
    panic("Bad format specifier for jupiter: %s", string);
  }

  if (tot_read < strlen(string)) {
    panic("Bad format specifier for jupiter: %s", string);
  }

  expr->num_cores = core;
  expr->num_pods = pod;
  expr->num_tors_per_pod = tpp;
  expr->num_aggs_per_pod = app;

  //info("Creating a jupiter network with: %d, %d, %d, %d, %f", core, pod, app, tpp, bw);

  return (struct network_t *)jupiter_network_create(core, pod, app, tpp, bw);
}


static struct network_t *expr_clone_network(struct expr_t *expr) {
  return jupiter_string_to_network(expr, expr->network_string);
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

  if        (MATCH("general", "traffic-test")) {
    expr->traffic_test = strdup(value);
  } else if (MATCH("general", "traffic-training")) {
    expr->traffic_training = strdup(value);
  } else if (MATCH("general", "mop-duration")) {
    expr->mop_duration = atoi(value);
  } else if (MATCH("predictor", "ewma-coeff")) {
    expr->ewma_coeff = atof(value);
  } else if (MATCH("criteria", "risk-violation")) {
    expr->risk_violation_cost = risk_violation_name_to_func(value);
  } else if (MATCH("criteria", "criteria-time")) {
    expr->criteria_time = risk_delay_name_to_func(value);
  } else if (MATCH("criteria", "criteria-length")) {
    expr->criteria_plan_length = risk_length_name_to_func(value);
  } else if (MATCH("criteria", "promised-throughput")) {
    expr->promised_throughput = atof(value);
  } else if (MATCH("general", "network")) {
    info("Parsing jupiter config: %s", value);
    expr->network = jupiter_string_to_network(expr, value);
    expr->network_string = strdup(value);
  } else if (MATCH_SECTION("upgrade")) {
    if (MATCH_NAME("switch-group")) {
      jupiter_add_upgrade_group(value, &expr->upgrade_list);
    } else if (MATCH_NAME("freedom")) {
      expr->upgrade_nfreedom = jupiter_add_freedom_degree(value, &expr->upgrade_freedom);
    } else {
      panic("Upgrading %s not supported.", name);
    }
  } else if (MATCH("cache", "rv-cache-dir")) {
    expr->cache.rvar_directory = strdup(value);
  } else if (MATCH("cache", "ewma-cache-dir")) {
    expr->cache.ewma_directory = strdup(value);
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
  expr->located_switches = sws;
  expr->nlocated_switches = nswitches;
}

static enum EXPR_ACTION
parse_action(char const *arg) {
  if      (strcmp(arg, "long-term") == 0) {
    info("Building long-term cache files.");
    return BUILD_LONGTERM;
  } else if (strcmp(arg, "pug") == 0) {
    info("Running PUG.");
    return RUN_PUG;
  } else if (strcmp(arg, "stg") == 0) {
    info("Running STG.");
    return RUN_STG;
  } else if (strcmp(arg, "ltg") == 0) {
    info("Running LTG.");
    return RUN_LTG;
  } else if (strcmp(arg, "cap") == 0) {
    info("Running CAP.");
    return RUN_CAP;
  } else if (strcmp(arg, "stats") == 0) {
    info("Running STATS.");
    return TRAFFIC_STATS;
  }

  panic("Invalid execution option: %s.", arg);
  return RUN_UNKNOWN;
}

static void
parse_duration(char const *arg, uint32_t *start, uint32_t *end) {
  char *ptr = strdup(arg);
  char *sbegin = ptr;

  while (*ptr != DURATION_SEPARATOR && *ptr != 0)
    ptr++;

  if (*ptr == 0)
    panic("Error parsing the duration: %s", arg);
  *ptr = 0;
  char *send = ptr + 1;

  *start = atoi(sbegin);
  *end = atoi(send);
  free(sbegin);

  info("Parsing duration: %d to %d", *start, *end);
}

static int
cmd_parse(int argc, char *const *argv, struct expr_t *expr) {
  int opt = 0;
  while ((opt = getopt(argc, argv, "a:r:")) != -1) {
    switch (opt) {
      case 'a':
        expr->action = parse_action(optarg);
        break;
      case 'd':
        parse_duration(optarg, 
            &expr->cache.subplan_start,
            &expr->cache.subplan_end);
        break;
    };
  }
  return 1;
};

int _step_count(struct criteria_time_t *crit, uint32_t length) {
  return (crit->steps >= length);
}

void config_parse(char const *ini_file, struct expr_t *expr, int argc, char *const *argv) {
  info("Parsing config %s", ini_file);
  if (ini_parse(ini_file, config_handler, expr) < 0) {
    panic("Couldn't load the ini file.");
  }
  if (cmd_parse(argc, argv, expr) < 0) {
    panic("Couldn't parse the command line options.");
  }

  expr->clone_network = expr_clone_network;
  _build_located_switch_group(expr);
}

