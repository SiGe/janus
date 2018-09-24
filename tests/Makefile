FILES=$(wildcard acu_test.h test_*.c ../*.c)
SRC=$(filter-out ../maxmin.c, $(FILES))

OBJ=$(SRC:.c=.o)
CC=gcc

CFLAGS=-g -O0
LDFLAGS = -g -O0

test: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)
	./test

.PHONY: clean test
clean:
	rm -f $(OBJ) test
