#ifndef _TWIDDLE_H_
#define _TWIDDLE_H_

/*
 * Inspired and for the most part copied from code from Matthew Belmonte
 * <mkb4@Cornell.edu>, 23 March 1996.  This implementation Copyright (c) 1996
 * by Matthew Belmonte.  Permission for use and distribution is hereby granted,
 * subject to the restrictions that this copyright notice and reference list be
 * included in its entirety, and that any and all changes made to the program
 * be clearly noted in the program text.

 * This software is provided 'as is', with no warranty, express or implied,
 * including but not limited to warranties of merchantability or fitness for a
 * particular purpose.  The user of this software assumes liability for any and
 * all damages, whether direct or consequential, arising from its use.  The
 * author of this implementation will not be liable for any such damages.

 * Reference:

 * Phillip J Chase, `Algorithm 382: Combinations of M out of N Objects [G6]',
 * Communications of the Association for Computing Machinery 13:6:368 (1970).

 * The returned indices x, y, and z in this implementation are decremented by 1,
 * in order to conform to the C language array reference convention.  Also, the
 * parameter 'done' has been replaced with a Boolean return value.
 *
 * twiddle.c - generate all combinations of M elements drawn without replacement
 * from a set of N elements.  This routine may be used in two ways:
 * (0) To generate all combinations of M out of N objects, let a[0..N-1]
 * contain the objects, and let c[0..M-1] initially be the combination
 * a[N-M..N-1].  While twiddle(&x, &y, &z, p) is false, set c[z] = a[x] to
 * produce a new combination.
 * (1) To generate all sequences of 0's and 1's containing M 1's, let
 * b[0..N-M-1] = 0 and b[N-M..N-1] = 1.  While twiddle(&x, &y, &z, p) is
 * false, set b[x] = 1 and b[y] = 0 to produce a new sequence.
 * In either of these cases, the array p[0..N+1] should be initialised as
 * follows:
 * p[0] = N+1
 * p[1..N-M] = 0
 * p[N-M+1..N] = 1..M
 * p[N+1] = -2
 * if M=0 then p[1] = 1
 * In this implementation, this initialisation is accomplished by calling
 * twiddle_init(M, N, p), where p points to an array of N+2 ints.
*/

struct twiddle_t {
  // Internal variables in Chase's algorithm;
	int x, y, z, *p, *b;
  int M, N, MN;
  int finished;

  // Tuple representation of the assignment.
  int *_tuple;
  int _tuple_size;

  void (*next)  (struct twiddle_t *);
  void (*begin) (struct twiddle_t *);
  int  (*end)   (struct twiddle_t *);
  void (*free)  (struct twiddle_t *);

  int  *(*tuple) (struct twiddle_t *);
  int   (*tuple_size) (struct twiddle_t *);
};

// Creates a twiddler trying to fit M balls into N bins
struct twiddle_t *twiddle_create(int M, int N);


#endif
