BUILD_DIR = build
BIN_DIR = bin
TARGET = main
#CC = clang
CC = gcc-8

SRC:=$(filter-out src/main.c src/test.c, $(wildcard src/*.c))
SRC+=$(wildcard src/algo/*.c)
SRC+=$(wildcard src/networks/*.c)
SRC+=$(wildcard src/util/*.c)

MAIN_SRC:=$(SRC)
MAIN_SRC+=src/main.c
MAIN_OBJ=$(MAIN_SRC:%.c=$(BUILD_DIR)/%.o)

TEST_SRC:=$(SRC)
TEST_SRC+=src/test.c
TEST_OBJ=$(TEST_SRC:%.c=$(BUILD_DIR)/%.o)

#OPT = -O0 -pg -g
OPT = -O3 -pg -g

CFLAGS=$(OPT)  -Wall -Werror -Iinclude/ -std=c11 -fms-extensions -Wno-microsoft-anon-tag
LDFLAGS=-lm $(OPT) -Wall -Werror -Iinclude/ -std=c11 -flto

all: $(TARGET)

$(BUILD_DIR)/%.o: %.c
	@>&2 echo Compiling $<
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(EXTRA) $(CPPFLAGS) -c -o $@ $<

$(TARGET): $(MAIN_OBJ)
	@>&2 echo Building $@
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

test: $(TEST_OBJ)
	@>&2 echo Building $@
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -f $(MAIN_OBJ) $(BUILD_DIR)/$(TARGET)
