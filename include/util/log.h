#ifndef _LOG_H_
#define _LOG_H_

#include "types.h"

#define MAX_FORMAT_LENGTH 256

#ifndef LOG_LEVEL
#define LOG_LEVEL 0
#endif

#define LOG_ERROR 1
#define LOG_INFO  2

void info(char const *fmt, ...);
void error(char const *fmt, ...);
void panic(char const *fmt, ...);
void network_print_flows(struct network_t *);

#endif
