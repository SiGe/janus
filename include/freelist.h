#ifndef _FREELIST_H_
#define _FREELIST_H_

#include <pthread.h>

/* TODO: Could possibly be a union? */
struct freelist_t {
  struct freelist_t *next;
  void *data;
};

/* A threadsafe free list 
 *
 * Items are kept in pairs of next/data and juggled between two chains
 * where one chain keeps the busy items and one keeps the free items.
 *
 * To use the freelist push a bunch of objects into it (using the _return) then
 * _get whenever you need one of those items.  One of the "free" objects will
 * be returned and is marked as occupied until your _return it back.
 */
struct freelist_repo_t {
  /* TODO: Bad naming fix later */
  struct freelist_t *free; // This is the list of items that have been borrowed
  struct freelist_t *busy; // This is the list of items that are "free"

  /* Mutex lock */
  pthread_mutex_t lock;

  /* Data in the freelist */
  struct freelist_t *_data;

  /* Number of items in the freelist */
  unsigned size;
};

/* Get a free pointer in the freelist */
void *freelist_get(struct freelist_repo_t *ntr);
void freelist_return(struct freelist_repo_t *ntr, void *net);

/* Returns the size of the freelist */
unsigned freelist_size(struct freelist_repo_t *ntr);

/* Create a free list of size size */
struct freelist_repo_t *freelist_create(unsigned size);

/* Free the memory occupied by the freelist.
 *
 * Does not release the items.  Make sure to _get all the items and free them
 * before _freeing the free list */
void freelist_free(struct freelist_repo_t *ntr);

#endif // _FREELIST_H_

