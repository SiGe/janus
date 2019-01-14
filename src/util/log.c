#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "util/log.h"
#include "dataplane.h"

#ifndef LOG_LEVEL
#define LOG_LEVEL 3
#endif

#define LOG(level, color) {\
  char format[MAX_FORMAT_LENGTH] = {0};\
  strcat(format, "\e[1;" color "m" #level " [%s:%d]" "\033[0m "); \
  strcat(format, fmt);\
  strcat(format, "\n");\
  va_list args;\
  va_start(args, fmt);\
  vfprintf(stderr, format, args);\
  va_end(args);\
}

#if LOG_LEVEL >= LOG_ERROR
void _error(const char *fmt, ...) LOG(ERROR, "91");
void _panic(const char *fmt, ...) {
  LOG(PANIC, "91");
  exit(EXIT_FAILURE);
}
#else
void _error(const char *fmt, ...) {}
void _panic(const char *fmt, ...) {
  exit(EXIT_FAILURE);
}
#endif


#if LOG_LEVEL >= LOG_INFO
void _info(const char *fmt, ...) LOG(INFO, "32");
#else
void _info(const char *fmt, ...) {}
#endif

#if LOG_LEVEL >= LOG_WARN
void _warn(const char *fmt, ...) LOG(WARN, "93");
#else
void _warn(const char *fmt, ...) {}
#endif


/*
// TODO jiaqi: you can change the output format here. both network->flows and
// network->links are in the same order that you passed. To see what the
// structures have take a look at types.h
void network_print_flows(struct dataplane_t *network) {
  printf("----------------------------------------\n");
  struct flow_t *flow = 0;
  for (int i = 0; i < network->num_flows; ++i) {
    flow = &network->flows[i];
    printf("flow %d: %.2f/%.2f (%.2f%%) -- %.2f.", i, flow->bw, flow->demand,
        flow->bw/flow->demand * 100, (flow->demand - flow->bw));
    printf("\n");
  }
  printf("----------------------------------------\n");
  for (int i = 0; i < network->num_links; ++i) {
    struct link_t *link = &network->links[i];
    printf("link %d: %.2f/%.2f (%.2f%%) -- %.2f\n", i, link->used,
        link->capacity, link->used/link->capacity * 100, (link->capacity -
          link->used)/link->nactive_flows);
  }
  printf("----------------------------------------\n");
}
*/
