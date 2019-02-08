#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "algo/maxmin.h"
#include "config.h"
#include "dataplane.h"
#include "exec/longterm.h"
#include "exec/ltg.h"
#include "exec/pug.h"
#include "exec/pug_long.h"
#include "exec/stats.h"
#include "freelist.h"
#include "network.h"
#include "plan.h"
#include "predictors/rotating_ewma.h"
#include "risk.h"
#include "util/common.h"
#include "util/log.h"

void usage(const char *fname) {
  const char *usage_message = ""
    "usage: %s <experiment-setting ini file> [OPTIONS]\n"
		"\nAvailable options:\n"
		"\t-a [ACTION]\t Choose an action: pug, pug-long, ltg, stats, long-term\n"
		"\t-x\t\t Explain the action\n"
		"";

  printf(usage_message, fname);
  exit(EXIT_FAILURE);
}

struct exec_t *executor(struct expr_t *expr) {
  if (expr->action == BUILD_LONGTERM) {
    return exec_longterm_create();
  } else if (expr->action == RUN_PUG) {
    return exec_pug_create();
  } else if (expr->action == RUN_PUG_LONG) {
    return exec_pug_long_create();
  } else if (expr->action == TRAFFIC_STATS) {
    return exec_traffic_stats_create();
  } else if (expr->action == RUN_LTG) {
    return exec_ltg_create();
  }

  panic("Executor not implemented.");
  return 0;
}

static void logo(void) {
  const char text[] = ""
 "  _   _      _  ______ _____ \n"
 " | \\ | |    | | | ___ \\  ___|\n"
 " |  \\| | ___| |_| |_/ / |__  \n"
 " | . ` |/ _ \\ __|    /|  __| \n"
 " | |\\  |  __/ |_| |\\ \\| |___ \n"
 " \\_| \\_/\\___|\\__\\_| \\_\\____/ \n"
 "  A Network Risk Estimator   \n";
/*
  const char text[] = ""
   " ________________________\n"
   "/ .  ..___.___..__ .___  \\\n"
   "  |\\ |[__   |  [__)[__    \n"
   "  | \\|[___  |  |  \\[___   \n"
   "\\________________________/\n"
   " A NETwork Risk Estimator\n"
   "                     \n";
*/
	printf("%s\n\n", text);
}

int main(int argc, char **argv) {
  logo();
  if (argc < 2) {
    usage(argv[0]);
    exit(1);
  }

  struct expr_t expr = {0};
  config_parse(argv[1], &expr, argc - 1, argv + 1);
  struct exec_t *exec = executor(&expr);

  if (!expr.explain) {
    exec->validate(exec, &expr);
    exec->run(exec, &expr);
  } else {
    exec->explain(exec);
  }

  return EXIT_SUCCESS;
}
