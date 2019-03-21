#ifndef _LOG_H_
#define _LOG_H_

#include "dataplane.h"

#define MAX_FORMAT_LENGTH 256

#define LOG_ERROR 1
#define LOG_WARN 2
#define LOG_INFO  3

void _info(char const *fmt, ...);
void _warn(char const *fmt, ...);
void _error(char const *fmt, ...);
void _panic(char const *fmt, ...);
void _network_print_flows(struct dataplane_t *);
void _text_block(char const *fmt, ...);

#define panic(fmt, ...) { \
  _panic(fmt, __FILE__, __LINE__, __VA_ARGS__); \
}

#define panic_txt(fmt) { \
  _panic(fmt, __FILE__, __LINE__); \
}


#define warn(fmt, ...) { \
  _warn(fmt, __FILE__, __LINE__, __VA_ARGS__); \
}

#define info(fmt, ...) { \
  _info(fmt, __FILE__, __LINE__, __VA_ARGS__); \
}

#define info_txt(fmt) { \
  _info(fmt, __FILE__, __LINE__); \
}

#define warning(fmt, ...) { \
  _warning(fmt, __FILE__, __LINE__, __VA_ARGS__); \
}

#define error(fmt, ...) { \
  _error(fmt, __FILE__, __LINE__, __VA_ARGS__); \
}

#define text_block(fmt, ...) { \
  _text_block(fmt, __VA_ARGS__, __FILE__, __LINE__); \
}

#define text_block_txt(fmt) { \
  _text_block(fmt, __FILE__, __LINE__); \
}

#endif
