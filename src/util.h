#ifndef UTIL_H
#define UTIL_H

#include <sys/time.h>
#include <stdio.h>
#include <netdb.h>

#include <string.h>
#include <errno.h>

#define perror(fmt, ...) fprintf(stderr, __FILE__ ":%05d: " fmt "\n",	\
				 __LINE__, ##__VA_ARGS__)

#ifdef DEBUG
#define print_dbg perror
#else
#define print_dbg(fmt, ...) ;
#endif

#define LEN(a) (sizeof(a)/sizeof(a[0]))

double time_elasped(struct timeval const*);
int nonblock_connect(struct sockaddr_in const*);

#endif
