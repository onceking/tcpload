#ifndef UTIL_H
#define UTIL_H

#include <sys/time.h>
#include <stdio.h>
#include <netdb.h>

#ifdef DEBUG
#define print_dbg(fmt, ...) fprintf(stderr, __FILE__ ":%05d: " fmt "\n", \
				    __LINE__, ##__VA_ARGS__)
#else
#define print_dbg(fmt, ...) ;
#endif

#define LEN(a) (sizeof(a)/sizeof(a[0]))

int nonblock_connect(struct sockaddr_in const* dst);
double time_elasped(struct timeval const* beg);

#endif
