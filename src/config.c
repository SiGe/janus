#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/common.h"
#include "inih/ini.h"
#include "failures/jupiter.h"
#include "networks/jupiter.h"
#include "risk.h"
#include "util/log.h"

#include "config.h"

static char* VERBOSITY_TEXT[] = {
  "[0] Enjoy the silence.",
  "[1] Room noise level",
  "[2] Coffeeshop noise level",
  "[3] Auditarium noise level",
  "[4] Show me everything (with cool graphs. kthx.)",
  "[5] No really, show me everything (graphs and kittens and donuts.)"
};

static struct risk_cost_func_t *risk_violation_name_to_func(char const *name) {
  return risk_cost_string_to_func(name);
}

static int _cutoff_at(struct criteria_time_t *ct, uint32_t length) {
  return ct->steps >= length;
}

static risk_cost_t _cutoff_zero_cost(struct criteria_time_t *ct, unsigned step) {
  return 0;
}

static risk_cost_t _cutoff_cost(struct criteria_time_t *ct, unsigned step) {
  if (step < 1)
    panic("Asking for the cost of a plan before it has started: %d", step);

  if (step > ct->steps) {
    panic("Cannot ask for the cost of cutoff after the number of allowed steps: %d > %d", step, ct->steps);
  }

  return ct->steps_cost[step - 1];
}

static struct criteria_time_t *risk_delay_name_to_func(char const *name) {
  char *ptr = strstr(name, "cutoff-at-");

  if (ptr == 0)
    panic("Unsupported criteria_time function: %s", name);

  ptr += strlen("cutoff-at-");
  struct criteria_time_t *ret = malloc(sizeof(struct criteria_time_t));
  ret->acceptable = _cutoff_at;

  char *tmp = strdup(ptr);
  char *cur = tmp;
  while (*cur != 0 && *cur != '/') {
    info("cur: %c", *cur);
    cur++;
  }

  if (*cur == 0) {
    info("No time cost provided assuming zero cost for time: %s", name);
    ret->steps = strtoul(tmp, 0, 0);
    ret->cost = _cutoff_zero_cost;
    free(tmp);
    return ret;
  }

  *cur = 0;
  ret->steps = strtoul(tmp, 0, 0);
  ret->steps_cost = malloc(sizeof(risk_cost_t) * ret->steps);

  // Read the rest of the 
  int idx = 0; cur++;
  char *pch = strtok(cur, ",");
  risk_cost_t sum = 0;
  while (pch != 0 && idx < ret->steps) {
    sum += atof(pch);
    ret->steps_cost[idx] = sum;
    pch = strtok(0, ",");
    idx++;
  }

  if (idx != ret->steps)
    panic("Not enough numbers passed to cutoff time criteria cost function.  Format is: cutoff-at-[NSTEPS]/STEP1_COST,STEP2_COST,...STEPN_COST: %s", name);

  if (pch != 0)
    panic("Too many numbers passed to cutoff time criteria cost function.  Format is: cutoff-at-[NSTEPS]/STEP1_COST,STEP2_COST,...STEPN_COST: %s", name);

  ret->cost = _cutoff_cost;

  return ret;
}

static int _cl_maximize(unsigned a1, unsigned a2) {
  if (a1 > a2) { return 1; }
  else if (a1 == a2) { return 0; }
  else if (a1 < a2) { return -1; }
  return -1;
}

static int _cl_minimize (unsigned a1, unsigned a2) {
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

static struct network_t *jupiter_string_to_network(struct expr_t const *expr, char const *string) {
  uint32_t core, pod, app, tpp; bw_t bw;
  int tot_read;

  if (sscanf(string, "jupiter-%d-%d-%d-%d-"BWF"%n", &core, &pod, &app, &tpp, &bw, &tot_read) <= 0) {
    panic("Bad format specifier for jupiter: %s", string);
  }

  if (tot_read < strlen(string)) {
    panic("Bad format specifier for jupiter: %s", string);
  }

  //info("Creating a jupiter network with: %d, %d, %d, %d, %f", core, pod, app, tpp, bw);

  return (struct network_t *)jupiter_network_create(core, pod, app, tpp, bw);
}

static void _expr_network_set_variables(struct expr_t *expr) {
  struct jupiter_network_t *jup = (struct jupiter_network_t *)expr->network;
  expr->network_type = NET_JUPITER;
  expr->num_cores = jup->core;
  expr->num_pods = jup->pod;
  expr->num_tors_per_pod = jup->tor;
  expr->num_aggs_per_pod = jup->agg;
}


static struct network_t *expr_clone_network(struct expr_t const *expr) {
  return jupiter_string_to_network(expr, expr->network_string);
}

static void jupiter_add_upgrade_group(char const *string, struct jupiter_sw_up_list_t *list) {
  int tot_read;
  char sw_type[256] = {0};
  unsigned location, count, color;

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
    up->type = JST_CORE;
  } else if (strcmp(sw_type, "pod/agg") == 0) {
    up->type = JST_AGG;
  } else {
    panic("Bad switch type specified: %s", sw_type);
  }
}

static uint32_t
jupiter_add_freedom_degree(char const *string,
    uint32_t **freedom) {
  unsigned ndegree = 1;
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
  } else if (MATCH("predictor", "type")) {
    expr->predictor_string = strdup(value);
  } else if (MATCH("criteria", "risk-violation")) {
    expr->risk_violation_cost = risk_violation_name_to_func(value);
  } else if (MATCH("criteria", "criteria-time")) {
    expr->criteria_time = risk_delay_name_to_func(value);
  } else if (MATCH("failure", "failure-mode")) {
    expr->failure_mode = strdup(value);
  } else if (MATCH("failure", "failure-warm-cost")) {
    expr->failure_warm_cost = atof(value);
  } else if (MATCH("failure", "concurrent-switch-failure")) {
    expr->failure_max_concurrent = strtoul(value, 0, 0);
  } else if (MATCH("failure", "concurrent-switch-probability")) {
    expr->failure_switch_probability = atof(value);
  } else if (MATCH("criteria", "criteria-length")) {
    expr->criteria_plan_length = risk_length_name_to_func(value);
  } else if (MATCH("criteria", "promised-throughput")) {
    expr->promised_throughput = atof(value);
  } else if (MATCH("pug", "backtrack-traffic-count")) {
    expr->pug_backtrack_traffic_count = atoi(value);
  } else if (MATCH("pug", "backtrack-direction")) {
    if (strcmp(value, "forward") == 0) {
      expr->pug_is_backtrack = 0;
    } else if (strcmp(value, "backward") == 0){
      expr->pug_is_backtrack = 1;
    } else {
      panic("Invalid [pug]->backtrack_direction: %s", value);
    }
  } else if (MATCH("general", "network")) {
    info("Parsing jupiter config: %s", value);
    expr->network_string = strdup(value);
    expr->network = jupiter_string_to_network(expr, value);
    _expr_network_set_variables(expr);
  } else if (MATCH_SECTION("upgrade")) {
    if (MATCH_NAME("switch-group")) {
      jupiter_add_upgrade_group(value, &expr->upgrade_list);
    } else if (MATCH_NAME("freedom")) {
      expr->upgrade_nfreedom = jupiter_add_freedom_degree(value, &expr->upgrade_freedom);
    } else {
      panic("Upgrading %s not supported.", name);
    }
  } else if (MATCH("scenario", "time-begin")) {
    expr->scenario.time_begin = strtoul(value, 0, 0);
  } else if (MATCH("scenario", "time-end")) {
    expr->scenario.time_end = strtoul(value, 0, 0);
  } else if (MATCH("scenario", "time-step")) {
    expr->scenario.time_step = strtoul(value, 0, 0);
  } else if (MATCH("cache", "rv-cache-dir")) {
    expr->cache.rvar_directory = strdup(value);
  } else if (MATCH("cache", "ewma-cache-dir")) {
    expr->cache.ewma_directory = strdup(value);
  } else if (MATCH("cache", "perfect-cache-dir")) {
    expr->cache.perfect_directory = strdup(value);
  }

  return 1;
}

static void
_jupiter_build_located_switch_group(struct expr_t *expr) {
  //TODO: Assume jupiter network for now.
  unsigned nswitches = 0;
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
      if (up->type == JST_CORE) {
        sws[idx].sid = jupiter_get_core(expr->network, j);
      } else if (up->type == JST_AGG) {
        sws[idx].sid = jupiter_get_agg(expr->network, up->location, j);
      } else {
        panic("Unsupported type for located_switch: %d", up->type);
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
    info_txt("Building long-term cache files.");
    return BUILD_LONGTERM;
  } else if (strcmp(arg, "pug") == 0) {
    info_txt("Running PUG.");
    return RUN_PUG;
  } else if (strcmp(arg, "pug-long") == 0) {
    info_txt("Running PUG LONG.");
    return RUN_PUG_LONG;
  } else if (strcmp(arg, "pug-lookback") == 0) {
    info_txt("Running PUG LOOKBACK.");
    return RUN_PUG_LOOKBACK;
  } else if (strcmp(arg, "stg") == 0) {
    info_txt("Running STG.");
    return RUN_STG;
  } else if (strcmp(arg, "ltg") == 0) {
    info_txt("Running LTG.");
    return RUN_LTG;
  } else if (strcmp(arg, "cap") == 0) {
    info_txt("Running CAP.");
    return RUN_CAP;
  } else if (strcmp(arg, "stats") == 0) {
    info_txt("Running STATS.");
    return TRAFFIC_STATS;
  }

  panic("Invalid execution option: %s.", arg);
  return RUN_UNKNOWN;
}

static int
cmd_parse(int argc, char *const *argv, struct expr_t *expr) {
  int opt = 0;
  expr->explain = 0;
  expr->verbose = 0;
  while ((opt = getopt(argc, argv, "a:r:vx")) != -1) {
    switch (opt) {
      case 'a':
        expr->action = parse_action(optarg);
        break;
      case 'x':
        expr->explain = 1;
      case 'v':
        expr->verbose += 1;
    };
  }
  return 1;
}

static void _jupiter_build_failure_model(struct expr_t *expr) {
  if (strcmp(expr->failure_mode, "warm") == 0) {
    expr->failure = (struct failure_model_t *)
      jupiter_failure_model_warm_create(
        expr->failure_max_concurrent,
        expr->failure_switch_probability,
        expr->failure_warm_cost);
  } else if (strcmp(expr->failure_mode, "independent") == 0) {
    expr->failure = (struct failure_model_t *)
      jupiter_failure_model_independent_create(
        expr->failure_max_concurrent,
        expr->failure_switch_probability);
  } else {
    panic("Invalid failure model: %s.", expr->failure_mode);
  }
}

/* Set some sensible defaults for experiments */
static void _expr_set_default_values(struct expr_t *expr) {
  expr->verbose = 0;
  expr->pug_is_backtrack = 1;
  expr->pug_backtrack_traffic_count = 10;
  expr->failure_max_concurrent = 0;
  expr->failure_switch_probability = 0;
  expr->failure_mode = 0;
  expr->failure_warm_cost = 0;
}

void config_parse(char const *ini_file, struct expr_t *expr, int argc, char *const *argv) {
  info("Parsing config %s", ini_file);
  _expr_set_default_values(expr);

  if (ini_parse(ini_file, config_handler, expr) < 0) {
    panic_txt("Couldn't load the ini file.");
  }
  if (cmd_parse(argc, argv, expr) < 0) {
    panic_txt("Couldn't parse the command line options.");
  }

  info("Using verbosity: %s", VERBOSITY_TEXT[expr->verbose]);

  expr->clone_network = expr_clone_network;
  if (expr->network_type == NET_JUPITER) {
    _jupiter_build_located_switch_group(expr);
    _jupiter_build_failure_model(expr);
  }
}

