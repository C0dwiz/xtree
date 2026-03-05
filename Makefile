# Makefile for xtree

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Iinclude -Wall -Wextra -O2
LDFLAGS ?=
LDLIBS ?=

SRCS := main.cpp $(filter-out src/xtree_tools/git_stub.cpp,$(wildcard src/xtree_tools/*.cpp))
OBJS := $(SRCS:%.cpp=build/%.o)

TARGET := xtree

.PHONY: all clean install dirs

all: $(TARGET)

dirs:
	@mkdir -p build

$(TARGET): dirs $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

build/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf build $(TARGET)

install: $(TARGET)
	install -d $(DESTDIR)/usr/local/bin
	install -m 0755 $(TARGET) $(DESTDIR)/usr/local/bin/$(TARGET)

show:
	@echo CXX=$(CXX)
	@echo CXXFLAGS=$(CXXFLAGS)
	@echo SRCS=$(SRCS)
	@echo OBJS=$(OBJS)
