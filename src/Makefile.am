bin_PROGRAMS = tragen tragen_debug

tragen_SOURCES = main.c util.c util.h req.c req.h
tragen_debug_SOURCES = main.c util.c util.h req.c req.h

CFLAGS = -fcolor-diagnostics -Qunused-arguments -g -ggdb
tragen_CFLAGS = -O2
tragen_debug_CFLAGS = -O0 -DDEBUG
