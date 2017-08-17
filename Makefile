SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
CC=gcc

OPT=-O3
EXTRA=-g -pg
EXTRA=

CFLAGS=-Wall -Werror $(OPT) $(EXTRA)
LDFLAGS = -lm -Wall -Werror $(OPT) $(EXTRA)

maxmin: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJ) maxmin
