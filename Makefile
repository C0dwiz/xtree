# Makefile for xtree

CXX ?= g++
CXXFLAGS ?= -std=c++17 -Iinclude -Wall -Wextra -O2
LDFLAGS ?=
LDLIBS ?=

SRCS := main.cpp $(filter-out src/xtree_tools/git_stub.cpp,$(wildcard src/xtree_tools/*.cpp))
OBJS := $(patsubst %.cpp,build/%.o,$(notdir $(SRCS)))

TARGET := xtree

.PHONY: all clean install dirs

all: $(TARGET)

dirs:
	@mkdir -p build

$(TARGET): dirs $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

define make_obj_rule
build/$(notdir $(1:.cpp=.o)): $(1)
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -c $(1) -o $$@
endef

$(foreach src,$(SRCS),$(eval $(call make_obj_rule,$(src))))

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
