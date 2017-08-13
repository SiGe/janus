SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

CFLAGS=-g -O3
LDFLAGS = -lm -O3

maxmin: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJ) maxmin
