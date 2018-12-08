BUILD_DIR = build
BIN_DIR = bin
TARGET = maxmin

SRC=$(wildcard *.c)
OBJ=$(SRC:%.c=$(BUILD_DIR)/%.o)

CFLAGS=-O3 -Wall -Werror
LDFLAGS=-lm -O3 -Wall -Werror

$(BUILD_DIR)/%.o: %.c
	@>&2 echo Compiling $<
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(EXTRA) $(CPPFLAGS) -c -o $@ $<

$(TARGET): $(OBJ)
	@>&2 echo Building $@
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(OBJ) $(BUILD_DIR)/$(TARGET)
