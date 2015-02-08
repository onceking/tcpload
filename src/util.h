#ifndef UTIL_H
#define UTIL_H

#include <sys/time.h>
#include <stdio.h>
#include <netdb.h>

#include <string.h>
#include <errno.h>
#include "stats.h"

#define perror(name)							\
	fprintf(stderr, __FILE__ ":%05d: " name "[%d]: %s\n",		\
		__LINE__, errno, strerror(errno))

#ifdef DEBUG
#define print_dbg(fmt, ...)					\
	fprintf(stderr, __FILE__ ":%05d: " fmt "\n",		\
		__LINE__, ##__VA_ARGS__)
#else
#define print_dbg(fmt, ...)
#endif

#define LEN(a) (sizeof(a)/sizeof(a[0]))

double time_elasped(struct timeval const*);
int nonblock_connect(struct sockaddr_in const*);

enum {
	WRITE_DONE = 0,
	WRITE_PARTIAL,
	WRITE_FAIL
};
int nonblock_write(int fd, char const*, long*, long, struct stats*);

#endif
