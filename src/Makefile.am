bin_PROGRAMS = tragen tragen_debug

tragen_SOURCES =       main.cpp util.cpp util.h req.cpp req.h poller.cpp
tragen_debug_SOURCES = main.cpp util.cpp util.h req.cpp req.h poller.cpp

CXXFLAGS = -fcolor-diagnostics -Qunused-arguments -g -ggdb
tragen_CXXFLAGS = -O2
tragen_debug_CXXFLAGS = -O0 -DDEBUG
