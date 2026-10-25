# StoryLang build.   make        -> build/storylang
#                    make test   -> build and run the tests
#                    make run    -> build and start the app
#
# SQLite: if third_party/sqlite3/sqlite3.c exists it is compiled into the program
# (no system install needed). Otherwise the system library is used (-lsqlite3;
# install libsqlite3-dev / sqlite-devel / `brew install sqlite`).

CXX      ?= g++
CC       ?= gcc
CXXFLAGS ?= -std=c++17 -Wall -Wextra -O2
CXXFLAGS += -Iinclude

AMALGAMATION := third_party/sqlite3/sqlite3.c
ifneq ($(wildcard $(AMALGAMATION)),)
  CXXFLAGS  += -Ithird_party/sqlite3
  SQLITE_OBJ := build/sqlite3.o
  LDLIBS    ?= -lpthread -ldl
else
  SQLITE_OBJ :=
  LDLIBS    ?= -lsqlite3
endif

SRCS      := $(wildcard src/*/*.cpp)
APP_SRC   := src/main.cpp
LIB_OBJS  := $(patsubst src/%.cpp,build/%.o,$(SRCS))
APP_OBJ   := build/main.o

all: build/storylang

build/storylang: $(LIB_OBJS) $(APP_OBJ) $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/storylang_tests: $(LIB_OBJS) build/tests/test_main.o $(SQLITE_OBJ)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDLIBS)

build/%.o: src/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/tests/test_main.o: tests/test_main.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/sqlite3.o: $(AMALGAMATION)
	@mkdir -p build
	$(CC) -O2 -DSQLITE_THREADSAFE=0 -c $< -o $@

test: build/storylang_tests
	./build/storylang_tests

run: build/storylang
	./build/storylang

clean:
	rm -rf build

.PHONY: all test run clean
