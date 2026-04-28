CXX ?= g++
CXXFLAGS ?= -std=c++17 -O3 -Wall -Wextra -pedantic -pthread -Iinclude

BUILD_DIR := build
LIB_SRCS := src/global_queue.cpp \
	src/abp_deque.cpp \
	src/chase_lev_deque.cpp \
	src/backend_factory.cpp \
	src/executor.cpp \
	src/nqueens.cpp
LIB_OBJS := $(patsubst src/%.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))

ifeq ($(OS),Windows_NT)
EXE := .exe
MKDIR_BUILD := if not exist $(subst /,\,$(BUILD_DIR)) mkdir $(subst /,\,$(BUILD_DIR))
RM_BUILD := if exist $(subst /,\,$(BUILD_DIR)) rmdir /S /Q $(subst /,\,$(BUILD_DIR))
else
EXE :=
MKDIR_BUILD := mkdir -p $(BUILD_DIR)
RM_BUILD := rm -rf $(BUILD_DIR)
endif

.PHONY: all bench tests test clean

all: bench tests

$(BUILD_DIR):
	$(MKDIR_BUILD)

bench: $(BUILD_DIR)/ws_bench$(EXE)

tests: $(BUILD_DIR)/ws_tests$(EXE)

$(BUILD_DIR)/%.o: src/%.cpp $(wildcard include/ws/*.hpp) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR)/ws_bench$(EXE): src/ws_bench.cpp $(LIB_OBJS) $(wildcard include/ws/*.hpp) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) src/ws_bench.cpp $(LIB_OBJS) -o $@

$(BUILD_DIR)/ws_tests$(EXE): tests/ws_tests.cpp $(LIB_OBJS) $(wildcard include/ws/*.hpp) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) tests/ws_tests.cpp $(LIB_OBJS) -o $@

test: tests
	$(BUILD_DIR)/ws_tests$(EXE)

clean:
	$(RM_BUILD)
