#ifndef THREAD_H
#define THREAD_H

#include <time.h>

#include "request.h"
#include "stats.h"

enum{
	THREAD_ST_BEGIN=0,

	THREAD_ST_DNS_SENDING,
	THREAD_ST_DNS_READING,

	THREAD_ST_CONNECTING,
	THREAD_ST_HEADER_SENDING,
	THREAD_ST_HEADER_READING,
	THREAD_ST_BODY_READING,
	THREAD_ST_DONE,
	THREAD_ST_ERROR,
	THREAD_ST_COUNT
};

struct thread{
	int epollfd;
	int peerfd;
	int write_pos;
	int read_pos;

	struct request* req;

	char resp[1<<10];
	int resp_len;

	int state;
	struct timeval state_time;

	struct stats stat;
};

char const* thread_state_string(int);
void thread_set_request(struct thread*, struct request*);
void thread_start(struct thread*);
void thread_process(struct thread*, struct epoll_event const*);
void thread_housekeep(struct thread*);

#endif
