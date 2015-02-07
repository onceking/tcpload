#ifndef POLLER_H
#define POLLER_H

#include <vector>
#include "req.h"
#include "stats.h"

struct poller {
	std::vector<struct request*> reqs;

	// async threads
	unsigned threads;

	int epollfd;
	std::vector<struct epoll_event> events;

	struct stats stat;
};

void poller_add(struct poller*, struct request*);

void poller_run(struct poller*, unsigned duration, unsigned trans);

#endif
