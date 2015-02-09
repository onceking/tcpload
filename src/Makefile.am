bin_PROGRAMS = tragen tragen_debug

tragen_SOURCES = main.cpp poller.cpp poller.h request.h stats.h thread.cpp thread.h util.cpp util.h
tragen_debug_SOURCES = $(tragen_SOURCES)

CXXFLAGS = -fcolor-diagnostics -Qunused-arguments -g -ggdb
tragen_CXXFLAGS = -O2
tragen_debug_CXXFLAGS = -O0 -DDEBUG
