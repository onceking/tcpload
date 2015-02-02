#include <sys/socket.h>
#include <sys/epoll.h>

#include "stats.h"
#include "poller.h"

struct poller {
	struct request* reqs;
	unsigned reqlen;

	// async threads
	int threads;

	int epollfd;
	struct epoll_event* events;
	unsigned eventlen;

	struct stats stat;
};

struct poller* poller_create(){
	struct poller* r = calloc(1, sizeof(struct poller));
	if(NULL != r){
		r.reqlen = 1024;
		r.reqs = calloc(r.reqlen, sizeof(struct request));


}
	return r;
}

void poller_add(struct req*){
}

//void poller_run(struct poller*, unsigned duration, unsigned trans);
//void poller_destroy(struct poller*);
