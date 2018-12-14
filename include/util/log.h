#ifndef _LOG_H_
#define _LOG_H_

#include "dataplane.h"

#define MAX_FORMAT_LENGTH 256

#define LOG_ERROR 1
#define LOG_INFO  2

void info(char const *fmt, ...);
void error(char const *fmt, ...);
void panic(char const *fmt, ...);
void network_print_flows(struct dataplane_t *);

#endif
