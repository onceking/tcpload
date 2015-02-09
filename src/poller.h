#ifndef POLLER_H
#define POLLER_H

#include <vector>
#include "request.h"
#include "thread.h"
#include "stats.h"

struct poller {
	int epollfd;
	std::vector<struct epoll_event> events;

	std::vector<struct thread> threads;

	struct stats stat;
};

void poller_add(struct poller*, struct request*);

void poller_run(struct poller*, unsigned duration, unsigned trans);

#endif
