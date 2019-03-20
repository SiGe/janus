#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "util/log.h"
#include "freelist.h"

/* Could deadlock if there aren't enough networks available. */
void *freelist_get(struct freelist_repo_t *ntr) {
  pthread_mutex_lock(&ntr->lock);

  struct freelist_t *rep = ntr->free;
  if (!rep)
    return 0;

  void *ret = ntr->free->data;
  ntr->free = ntr->free->next;

  rep->next = ntr->busy;
  rep->data = 0;
  ntr->busy = rep;

  pthread_mutex_unlock(&ntr->lock);
  return ret;
}

void freelist_return(struct freelist_repo_t *ntr, void *net) {
  pthread_mutex_lock(&ntr->lock);
  if (!ntr->busy)
    panic_txt("Not enough space left in the return chain.");

  struct freelist_t *rep = ntr->busy;

  // Fix the ntr busy chain
  ntr->busy = ntr->busy->next;

  // attach the link to the XX
  rep->next = ntr->free;
  rep->data = net;
  ntr->free = rep;

  pthread_mutex_unlock(&ntr->lock);
}

struct freelist_repo_t *freelist_create(unsigned num) {
  struct freelist_repo_t *repo = malloc(sizeof(struct freelist_repo_t));
  size_t size = sizeof(struct freelist_t) * num;
  repo->busy = malloc(size);
  repo->size = num;

  // For freeing later
  repo->_data = repo->busy;
  memset(repo->busy, 0, size);

  for (uint32_t i = 0; i < num-1; ++i) {
    repo->busy[i].next = &repo->busy[i+1];
    repo->busy[i].data = 0;
  }
  repo->busy[num-1].next = 0;
  repo->free = 0;

  if (pthread_mutex_init(&repo->lock, 0) != 0) {
    panic("Couldn't create the pthread mutex: %p", &repo->lock);
  }

  return repo;
}

void freelist_free(struct freelist_repo_t *repo){
  free(repo->_data);
  free(repo);
}

unsigned freelist_size(struct freelist_repo_t *ntr) {
  return ntr->size;
}
