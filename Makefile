OSCFLAGS=
OSLDFLAGS=

ifeq ($(OS),Windows_NT)
		exit -1
else
    UNAME_S := $(shell uname -s)
    ifeq ($(UNAME_S),Linux)
				OSCFLAGS+=-pthread
				OSLDFLAGS+=-pthread
    endif
    ifeq ($(UNAME_S),Darwin)
				OSCFLAGS+=-pthread
    endif
endif

BUILD_DIR = build
BIN_DIR = bin
TARGET = main
#BINARY = netre-debug
BINARY = netre

#CC = clang
CC = gcc
STD=gnu11

#OPT = -O0 -pg -g
#OPT = -O3 -pg -g
OPT = -O3

SRC:=$(filter-out src/traffic_compressor.c src/main.c src/test.c, \
	$(wildcard src/*.c src/*/*.c src/*/*/*.c lib/*/*.c))

MAIN_SRC:=$(SRC)
MAIN_SRC+=src/main.c
MAIN_OBJ=$(MAIN_SRC:%.c=$(BUILD_DIR)/%.o)

TRAFFIC_COMPRESSOR_SRC:=$(SRC)
TRAFFIC_COMPRESSOR_SRC+=src/traffic_compressor.c
TRAFFIC_COMPRESSOR_OBJ=$(TRAFFIC_COMPRESSOR_SRC:%.c=$(BUILD_DIR)/%.o)

TEST_SRC:=$(SRC)
TEST_SRC+=src/test.c
TEST_OBJ=$(TEST_SRC:%.c=$(BUILD_DIR)/%.o)

ifeq (test, $(firstword $(MAKECMDGOALS)))
OPT = -O0 -pg -g
endif

CFLAGS=$(OPT) $(DEFINE) -Wall -Werror \
			-pedantic -Wsign-conversion\
			-Wno-unused-function\
			-Iinclude/ -std=$(STD) -Ilib/ \
			-fms-extensions \
			-mtune=native $(OSCFLAGS)

LDFLAGS=-lm $(OPT) $(DEFINE) -Wall -Werror\
				-pedantic -Wsign-conversion\
				-Wno-unused-function -Wno-nested-anon-types -Wno-keyword-macro\
			 	-Iinclude/ -Ilib/ -std=$(STD) -flto \
				-mtune=native $(OSLDFLAGS)

ifeq (gcc, $(CC))
	LDFLAGS += -pthread
else ifeq (clang, $(CC))
	CFLAGS += -Wno-nested-anon-types -Wno-keyword-macro -Wno-microsoft-anon-tag
endif


all: $(TARGET)

$(BUILD_DIR)/%.o: %.c
	@>&2 echo Compiling $<
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(EXTRA) $(CPPFLAGS) -c -o $@ $<

$(TARGET): $(MAIN_OBJ)
	@>&2 echo Building $(BINARY)
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$(BINARY) $^ $(LDFLAGS)

test: $(TEST_OBJ)
	@>&2 echo Building $@
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

traffic_compressor: $(TRAFFIC_COMPRESSOR_OBJ)
	@>&2 echo Building $@
	@mkdir -p $(BIN_DIR)
	@$(CC) -o $(BIN_DIR)/$@ $^ $(LDFLAGS)

.PHONY: clean
clean:
	rm -fr $(BUILD_DIR)/*
