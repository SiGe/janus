SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
CC=gcc

OPT=-O1

CFLAGS=-Wall -Werror $(OPT) -pg -g
LDFLAGS = -lm -Wall -Werror $(OPT) -pg -g

maxmin: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJ) maxmin
