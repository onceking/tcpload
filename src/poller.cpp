#include <sys/socket.h>
#include <sys/epoll.h>

#include "poller.h"
#include "util.h"
#include "stats.h"

void poller_run(struct poller* p, unsigned duration, unsigned trans){
	p->epollfd = epoll_create1(EPOLL_CLOEXEC);
	if(p->epollfd == -1){
		perror("epoll_create: %s", strerror(errno));
		return;
	}

	memset(&p->stat, 0, sizeof(p->stat));
	time(&p->stat.beg);
	while(1){
		struct timeval poll_beg;
		double dur = 0.;
		int i;

		for(i=0; i<p->reqs.size(); ++i){
			// request_housekeep(p->reqs[i], &p->stat);
		}

		gettimeofday(&poll_beg, NULL);
		do{
			int n = epoll_wait(
				p->epollfd,
				&p->events[0], p->events.size(),
				110);
			print_dbg("%d events ready", n);
			for(i=0; i<n; ++i){
				struct request* r = (struct request*)p->events[i].data.ptr;
				request_process(r, p->epollfd); //, &p->stat);

			}
			dur = time_elasped(&poll_beg);
		}while(dur < 0.1);

		dur = time(NULL) - p->stat.beg;
		printf("Time: %0.2fs  Req: %u %.2f/s TX: %lu %.2f/s RX: %lu %.2f/s\n",
		       dur,
		       p->stat.count, p->stat.count/dur,
		       p->stat.tx, p->stat.tx/dur,
		       p->stat.rx, p->stat.rx/dur);
	}
}
