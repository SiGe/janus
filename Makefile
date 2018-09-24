SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
CC=gcc

OPT=-O3 -std=c99
EXTRA= -pthread

CFLAGS= $(OPT) $(EXTRA)
LDFLAGS = -lm  $(OPT) $(EXTRA)

maxmin: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJ) maxmin

new: clean maxmin
