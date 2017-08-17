SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)
CC=/usr/local/bin/gcc-7

CFLAGS=-p -pg -Wall -Werror -g
LDFLAGS = -lm -p -pg -Wall -Werror -g

maxmin: $(OBJ)
	$(CC) $(LDFLAGS) -o $@ $^

.PHONY: clean
clean:
	rm -f $(OBJ) maxmin
