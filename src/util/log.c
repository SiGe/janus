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
  strcat(format, "\x1B[1;" color "m" #level " [%s:%d]" "\033[0m "); \
  strcat(format, fmt);\
  strcat(format, "\n");\
  va_list args;\
  va_start(args, fmt);\
  vfprintf(stderr, format, args);\
  va_end(args);\
}

#if LOG_LEVEL >= LOG_ERROR
void _error(const char *fmt, ...) LOG(ERROR, "91")
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
void _info(const char *fmt, ...) LOG(INFO, "32")
#else
void _info(const char *fmt, ...) {}
#endif

#if LOG_LEVEL >= LOG_WARN
void _warn(const char *fmt, ...) LOG(WARN, "93")
#else
void _warn(const char *fmt, ...) {}
#endif

void _text_block(const char *fmt, ...) {
  char format[MAX_FORMAT_LENGTH] = {0};\
  strcat(format, "\x1B[46;4;30m\n"); \
  strcat(format, fmt);\
  strcat(format, "\033[0m\n");\
  strcat(format, "\033[1;33mINVOKED from [%s:%d]\033[0m\n");
  va_list args;\
  va_start(args, fmt);\
  vfprintf(stderr, format, args);\
  va_end(args);\
}
