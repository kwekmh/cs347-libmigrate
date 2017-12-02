SRCDIR = src
TESTDIR = tests
OBJDIR = obj
LIBDIR = lib
BINDIR = bin
SRCS = $(wildcard $(SRCDIR)/*.cc)
OBJS = $(SRCS:$(SRCDIR)/%.cc=$(OBJDIR)/%.o)
TESTSRCS = $(wildcard $(TESTDIR)/*.cc)
TESTS = $(TESTSRCS:$(TESTDIR)/%.cc=$(BINDIR)/%)
CXX = g++
CXXFLAGS = -g -Wall -std=c++11
LDFLAGS = -lpthread
BUILDDIRS = $(OBJDIR) $(BINDIR) $(LIBDIR)

default: build

build: create_dirs libmigrate.a

$(OBJS): $(OBJDIR)/%.o: $(SRCDIR)/%.cc
	$(CXX) -c -o $@ $< $(CXXFLAGS)

libmigrate.a: $(OBJS)
	ar -rv $(LIBDIR)/$@ $(OBJS)

tests: build
	$(CXX) -I./src -o $(TESTS) $(TESTSRCS) $(CXXFLAGS) -L./lib -lmigrate $(LDFLAGS)

create_dirs:
	@mkdir -p $(BUILDDIRS)

.PHONY: clean

clean:
	rm -rf $(LIBDIR)/* $(OBJS) $(TESTS)
