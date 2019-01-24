#ifndef _EXECUTOR_H_
#define _EXECUTOR_H_

struct expr_t;

struct exec_t {
  void (*validate) (struct exec_t *, struct expr_t const *expr);
  void (*run)      (struct exec_t *, struct expr_t *expr);
};

#include "util/log.h"

#define EXEC_VALIDATE_PTR_SET(e, p)    { if (e->p == 0) {panic("Need to set "#p);} }
#define EXEC_VALIDATE_STRING_SET(e, p) { if (e->p == 0 || strlen(e->p) == 0) {panic("Need to set string value "#p);} }

#endif // 
