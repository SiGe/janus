#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "twiddle.h"

void _twiddle_begin(struct twiddle_t *t) {
  memset(t->b, 0, sizeof(int) * (unsigned)t->MN);
  memset(t->p, 0, sizeof(int) * (unsigned)(t->MN + 2));
  memset(t->_tuple, 0, sizeof(int) * (unsigned)t->N);

  t->x = 0; t->y = 0;
  int m  = t->M;
  int n  = t->MN; // Yes, this is correct.
  int *p = t->p;
  int *b = t->b;

  t->finished = 0;

  for (int i = n - m; i != n; ++i) {
    b[i] = 1;
  }

  p[0] = n + 1;
  /* We have already zeroed out p:
	for(i = 1; i != n-m+1; i++)
		p[i] = 0;
  */

  for (int i = n-m+1;  i !=  n+1; ++i) {
    p[i] = i + m - n;
  }
  p[n+1] = -2;

  // God forbid comments
  if (m == 0)
    p[1] = 1;
}

void _twiddle_next(struct twiddle_t *t) {
  int *x, *y, *z, *p, *b;
  x = &t->x; y = &t->y; z = &t->z; p = t->p; b = t->b;

	register int i, j, k;
	j = 1;
	while(p[j] <= 0)
		j++;
	if(p[j-1] == 0)
	{
		for(i = j-1; i != 1; i--)
			p[i] = -1;
		p[j] = 0;
		*x = *z = 0;
		p[1] = 1;
		*y = j-1;
	}
	else
	{
		if(j > 1)
			p[j-1] = 0;
		do
			j++;
		while(p[j] > 0);
		k = j-1;
		i = j;
		while(p[i] == 0)
			p[i++] = -1;
		if(p[i] == -1)
		{
			p[i] = p[k];
			*z = p[k]-1;
			*x = i-1;
			*y = k-1;
			p[k] = -1;
		}
		else
		{
			if(i == p[0]) {
        b[*x] = 1;
        b[*y] = 0;
        t->finished = 1;
        return;
      }
			else
			{
				p[j] = p[i];
				*z = p[i]-1;
				p[i] = 0;
				*x = j-1;
				*y = i-1;
			}
		}
	}
  b[*x] = 1;
  b[*y] = 0;
}

int _twiddle_end(struct twiddle_t *t) {
  return t->finished;
}

void _twiddle_free(struct twiddle_t *t) {
  free(t->p);
  free(t->b);
  free(t->_tuple);
  free(t);
}

unsigned *_twiddle_tuple(struct twiddle_t *t) {
  int *b = t->b;
  int n = t->MN; unsigned *tuple = t->_tuple;
  unsigned ts = t->_tuple_size;

  int idx = 0;
  unsigned count = 0;

  for (int i = 0; i < n; ++i) {
    if (b[i] == 1) {
      count += 1;
    } else {
      tuple[idx] = count;
      count = 0;
      idx += 1;
    }
  }
  tuple[ts-1] = count;
  return tuple;
}

unsigned _twiddle_tuple_size(struct twiddle_t *t) {
  return t->_tuple_size;
}

struct twiddle_t *twiddle_create(int M, int N) {
  struct twiddle_t *ret = malloc(sizeof(struct twiddle_t));
  ret->M = M;
  ret->N = N;

  // Number of bins
  ret->MN = N + M - 1;

  ret->p      = malloc(sizeof(int) * (unsigned)(ret->MN + 2));
  ret->b      = malloc(sizeof(int) * (unsigned)(ret->MN));
  ret->_tuple = malloc(sizeof(unsigned) * (unsigned)(ret->N));
  ret->_tuple_size = (unsigned)ret->N;

  // Function pointers
  ret->tuple = _twiddle_tuple;
  ret->tuple_size = _twiddle_tuple_size;
  ret->begin = _twiddle_begin;
  ret->free = _twiddle_free;
  ret->next = _twiddle_next;
  ret->end = _twiddle_end;

  return ret;
}
