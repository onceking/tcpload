#include <sys/socket.h>
#include <sys/epoll.h>

#include "poller.h"
#include "util.h"
#include "stats.h"

void poller_add(struct poller* p, struct request* r){
	p->reqs.push_back(r);

}

void poller_run(struct poller* p, unsigned duration, unsigned trans){
	double dur = 0.;
	p->epollfd = epoll_create1(EPOLL_CLOEXEC);
	if(p->epollfd == -1){
		perror("epoll_create: %s", strerror(errno));
		return;
	}

	p->events.resize(p->reqs.size());
	time(&p->stat.beg);

	while((trans == 0 || p->stat.count > trans) &&
	      (duration == 0 || duration > dur)){
		struct timeval poll_beg;
		int i;

		for(i=0; i<p->reqs.size(); ++i){
			request_housekeep(p->reqs[i], p->epollfd); //, &p->stat);
		}

		gettimeofday(&poll_beg, NULL);
		do{
			int n = epoll_wait(
				p->epollfd,
				&p->events[0], p->events.size(),
				110);
			// print_dbg("%d events ready", n);
			for(i=0; i<n; ++i){
				struct epoll_event& ev = p->events[i];
				struct request* r = (struct request*)ev.data.ptr;
				print_dbg("Event: %0x", ev.events);
				request_process(r, &ev, p->epollfd); //, &p->stat);
			}
			dur = time_elasped(&poll_beg);
		}while(dur < 1);

		dur = time(NULL) - p->stat.beg;

		p->stat.tx = 0;
		p->stat.rx = 0;
		p->stat.txn = 0;
		p->stat.rxn = 0;
		p->stat.count = 0;
		for(i=0; i<p->reqs.size(); ++i){
			struct stats const* s = request_stat(p->reqs[i]);
			p->stat.tx += s->tx;
			p->stat.txn += s->txn;
			p->stat.rx += s->rx;
			p->stat.rxn += s->rxn;
			p->stat.count += s->count;
		}

		printf("Time: %0.2fs  Req: %lu %.2f/s"
		       " TX: %luKB/%lu %.0fkB/%.0f/s %lu/t"
		       " RX: %luMB/%luk %.0fMB/%.0fk/s %lu/r\n",
		       dur, p->stat.count, p->stat.count/dur,

		       p->stat.tx >> 10, p->stat.txn,
		       (p->stat.tx >> 10)/dur, p->stat.txn/dur,
		       p->stat.tx/p->stat.txn,

		       p->stat.rx >> 20, p->stat.rxn >> 10,
		       (p->stat.rx >> 20)/dur, (p->stat.rxn >> 10)/dur,
		       p->stat.rx/p->stat.rxn);
	}
}
