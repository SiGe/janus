SRC=$(wildcard *.c)
OBJ=$(SRC:.c=.o)

PROFILE=-O1 -pg -g
SPEED=-O3

MODE=$(SPEED)

CFLAGS=$(MODE) -Wall -Werror
LDFLAGS =$(MODE) -lm -Wall -Werror

maxmin: $(OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJ) maxmin
