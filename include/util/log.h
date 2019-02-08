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

#define panic(fmt, args...) { \
  _panic(fmt, __FILE__, __LINE__, ##args); \
}


#define warn(fmt, args...) { \
  _warn(fmt, __FILE__, __LINE__, ##args); \
}

#define info(fmt, args...) { \
  _info(fmt, __FILE__, __LINE__, ##args); \
}

#define warning(fmt, args...) { \
  _warning(fmt, __FILE__, __LINE__, ##args); \
}

#define error(fmt, args...) { \
  _error(fmt, __FILE__, __LINE__, ##args); \
}

#define text_block(fmt, args...) { \
  _text_block(fmt, ##args, __FILE__, __LINE__); \
}

#endif
