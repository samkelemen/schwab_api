# Makefile — libschwab_api.a  +  demos example1 … example9
# ──────────────────────────────────────────────────────────────
#  layout:
#     include/schwab_api.hpp
#     src/*.cpp
#     examples/example1.cpp … examples/example9.cpp
#
#  Usage:
#     make example5   # just that one demo
#     make all        # every demo + library
#     make clean
# -------------------------------------------------------------------

CXX       := g++
CXXFLAGS  := -std=c++20 -Wall -Wextra -O2 \
             -Iinclude -Isrc                             \
             $(shell pkg-config --cflags libcurl)
LDFLAGS   := $(shell pkg-config --libs   libcurl)

# library sources / objects ------------------------------------------
LIB_SRC := $(wildcard src/*.cpp)
OBJDIR  := build
LIB_OBJ := $(patsubst src/%.cpp,$(OBJDIR)/%.o,$(LIB_SRC))
LIB     := libschwab_api.a

# demo sources / objects / executables -------------------------------
DEMO_SRC := $(wildcard examples/example?.cpp)          # example1 … example9
DEMO_OBJ := $(patsubst examples/%.cpp,$(OBJDIR)/%.o,$(DEMO_SRC))
DEMOS    := $(patsubst %.cpp,%,$(notdir $(DEMO_SRC)))  # => example1 … example9

# default rule -------------------------------------------------------
all: $(LIB) $(DEMOS)

# static library -----------------------------------------------------
$(LIB): $(LIB_OBJ)
	@echo "[AR]  $@"
	ar rcs $@ $^

# build any exampleN on demand --------------------------------------
$(DEMOS): %: $(LIB) $(OBJDIR)/%.o
	@echo "[LD]  $@"
	$(CXX) $(OBJDIR)/$*.o -L. -lschwab_api -o $@ $(LDFLAGS)

# pattern rules for object files ------------------------------------
$(OBJDIR)/%.o: src/%.cpp include/schwab_api.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(OBJDIR)/%.o: examples/%.cpp include/schwab_api.hpp | $(OBJDIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# make sure build directory exists
$(OBJDIR):
	@mkdir -p $@

# cleanup ------------------------------------------------------------
clean:
	rm -rf $(OBJDIR) $(LIB) $(DEMOS)

.PHONY: all clean
