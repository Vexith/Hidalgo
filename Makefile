CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra -std=c11 -D_GNU_SOURCE -pthread -Iinclude
LDFLAGS ?= -pthread

SRC_DIR   := src
TESTS_DIR := tests
BUILD_DIR := build
LIB       := libhidalgo.a
TARGET    := hidalgo

SRCS := $(filter-out $(SRC_DIR)/main.c, $(wildcard $(SRC_DIR)/*.c))
OBJS := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

all: $(LIB) $(TARGET)

$(LIB): $(OBJS)
	ar rcs $@ $^

$(TARGET): $(LIB) $(SRC_DIR)/main.c
	$(CC) $(CFLAGS) $(SRC_DIR)/main.c $(LIB) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

test: $(LIB) $(TESTS_DIR)/hidalgo-cli.c
	$(CC) $(CFLAGS) $(TESTS_DIR)/hidalgo-cli.c $(LIB) -o hidalgocli $(LDFLAGS)
	./hidalgo-cli

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(BUILD_DIR) $(LIB) $(TARGET) test_index

-include $(DEPS)

.PHONY: all clean run test
