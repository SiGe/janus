#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include "log.h"

#define LOG(level, color) {\
  char format[MAX_FORMAT_LENGTH] = {0};\
  strcat(format, "\e[1;" color "m" #level ">\033[0m "); \
  strcat(format, fmt);\
  strcat(format, "\n");\
  va_list args;\
  va_start(args, fmt);\
  vfprintf(stderr, format, args);\
  va_end(args);\
}

#if LOG_LEVEL >= LOG_ERROR
void error(const char *fmt, ...) LOG(ERROR, "91");
void panic(const char *fmt, ...) {
  LOG(PANIC, "91");
  exit(EXIT_FAILURE);
}
#else
void error(const char *fmt, ...) {}
void panic(const char *fmt, ...) {
  exit(EXIT_FAILURE);
}
#endif


#if LOG_LEVEL >= LOG_INFO
void info(const char *fmt, ...) LOG(INFO, "32");
#else
void info(const char *fmt, ...) {}
#endif

