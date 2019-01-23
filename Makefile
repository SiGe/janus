BUILD_DIR = build
BIN_DIR = bin
BENCH_DIR = tests/benchmarks
TARGET = main

CC = clang
#CC = gcc

#OPT = -O0 -pg -g
OPT = -O3

SRC:=$(filter-out src/traffic_compressor.c src/main.c src/test.c, $(wildcard src/*.c))
SRC+=$(wildcard src/*/*.c)
SRC+=$(wildcard lib/*/*.c)

#SRC+=$(wildcard src/algo/*.c)
#SRC+=$(wildcard src/networks/*.c)
#SRC+=$(wildcard src/util/*.c)

MAIN_SRC:=$(SRC)
MAIN_SRC+=src/main.c
MAIN_OBJ=$(MAIN_SRC:%.c=$(BUILD_DIR)/%.o)

TRAFFIC_COMPRESSOR_SRC:=$(SRC)
TRAFFIC_COMPRESSOR_SRC+=src/traffic_compressor.c
TRAFFIC_COMPRESSOR_OBJ=$(TRAFFIC_COMPRESSOR_SRC:%.c=$(BUILD_DIR)/%.o)

TEST_SRC:=$(SRC)
TEST_SRC+=src/test.c
TEST_OBJ=$(TEST_SRC:%.c=$(BUILD_DIR)/%.o)

# Handle benchmark executions
BENCH_SRC:=$(SRC)
ifeq (bench, $(firstword $(MAKECMDGOALS)))
BENCH_TARGET=$(wordlist 2, 2, $(MAKECMDGOALS))
BENCH_SRC+=$(wildcard $(BENCH_DIR)/src/*.c)
BENCH_SRC+=$(BENCH_DIR)/$(BENCH_TARGET).c
BENCH_OBJ=$(BENCH_SRC:%.c=$(BUILD_DIR)/%.o)
CFLAGS+=-I$(BENCH_DIR)/include
endif

ifeq (test, $(firstword $(MAKECMDGOALS)))
OPT = -O0 -pg -g
endif

CFLAGS=$(OPT)  -Wall -Werror -Iinclude/ -std=gnu11 -fms-extensions -Wno-microsoft-anon-tag -Ilib/ -pthread
LDFLAGS=-lm $(OPT) -Wall -Werror -Iinclude/ -std=gnu11 -flto -Ilib/ -pthread


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

traffic_compressor: $(TRAFFIC_COMPRESSOR_OBJ)
	@>&2 echo Building $@
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

$(BENCH_TARGET): $(BENCH_OBJ)
	@>&2 echo Building $@
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

bench: $(BENCH_OBJ)
	@>&2 echo $(MAKECMDGOALS)

.PHONY: clean
clean:
	rm -fr $(BUILD_DIR)/*
