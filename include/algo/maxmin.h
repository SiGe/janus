#ifndef _ALGO_MAXMIN_H_
#define _ALGO_MAXMIN_H_

#include "dataplane.h"

/* Returns the flows under max-min fairness
 *
 * TODO: I can probably switch this with a simulator class which accepts an
 * input and returns packet-loss/impact?  Still not sure if that's the best
 * model.
 */
int maxmin(struct dataplane_t *);

#endif // _ALGO_MAXMIN_H_
