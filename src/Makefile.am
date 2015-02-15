bin_PROGRAMS = tragen tragen_debug

tragen_SOURCES = main.cpp poller.cpp thread.cpp util.cpp stats.cpp
tragen_debug_SOURCES = $(tragen_SOURCES)

CXXFLAGS = -fcolor-diagnostics -Qunused-arguments
CXXFLAGS += -g -ggdb
tragen_CXXFLAGS = -O2
tragen_debug_CXXFLAGS = -O0 -DDEBUG
