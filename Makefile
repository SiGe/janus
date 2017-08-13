SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

CFLAGS=-O3 -Wall -Werror
LDFLAGS = -lm -O3 -Wall -Werror

maxmin: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJ) maxmin
