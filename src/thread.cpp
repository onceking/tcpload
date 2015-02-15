#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "thread.h"
#include "util.h"

#ifdef DEBUG
#define thread_set_state(t, s) do{				\
	print_dbg("%p: advanced from [%d]%s to [%d]%s. ",	\
		  (t),						\
		  (t)->state, thread_state_string((t)->state),	\
		  (s), thread_state_string(s));			\
	(t)->state = s;						\
	gettimeofday(&(t)->state_time, NULL);			\
	}while(0)

#define thread_goto_state(t, s) do{		\
	thread_set_state((t), (s));		\
	thread_process((t), NULL);		\
	}while(0)
#else
static void thread_set_state(struct thread* t, int s){
	print_dbg("%p: advanced from [%d]%s to [%d]%s. ",
		  t,
		  t->state, thread_state_string(t->state),
		  s, thread_state_string(s));
	t->state = s;
	gettimeofday(&t->state_time, NULL);
}
static void thread_goto_state(struct thread* t, int s){
	thread_set_state(t, s);
	thread_process(t, NULL);
}
#endif

static void sf_begin(struct thread*);
static void sf_connecting(struct thread*);
static void sf_header_sending(struct thread*);
static void sf_header_reading(struct thread*);
static void sf_done(struct thread*);
static void sf_error(struct thread*);

char const* thread_state_string(int st){
	static char const* strs[] = {
		"BEGIN",
		"SENDING_DNS",
		"READING_DNS",
		"CONNECTING",
		"SENDING_HEADER",
		"READING_HEADER",
		"READING_BODY",
		"DONE",
		"ERROR"
	};
	assert(LEN(strs) == THREAD_ST_COUNT);
	assert(st < THREAD_ST_COUNT);

	return strs[st];
}

void thread_set_request(struct thread* t, struct request* r){
	assert(t->state == THREAD_ST_BEGIN);
	if(t->req){
		stats_add(&t->req->stat, &t->stat);
		stats_clear(&t->stat);
	}
	t->req = r;
}

void thread_start(struct thread* t){
	assert(t->state == THREAD_ST_BEGIN);
	thread_process(t, NULL);
}

void thread_process(struct thread* t, struct epoll_event const* ev){
	static void (*fns[])(struct thread*) = {
		sf_begin,
		NULL,
		NULL,
		sf_connecting,
		sf_header_sending,
		sf_header_reading,
		NULL,
		sf_done,
		sf_error
	};
	assert(fns[t->state]);
	fns[t->state](t);
}

void thread_housekeep(struct thread* t){
	if(time_elasped(&t->state_time) > t->req->timeout_ms){
		thread_goto_state(t, THREAD_ST_ERROR);
	}
}

static void sf_begin(struct thread* t){
	t->peerfd = nonblock_connect(&t->req->dst);
	if(t->peerfd >= 0){
		struct epoll_event ev;
		ev.events = EPOLLOUT | EPOLLHUP;
		ev.data.ptr = (void*)t;
		if(0 == epoll_ctl(t->epollfd, EPOLL_CTL_ADD, t->peerfd, &ev)){
			thread_set_state(t, THREAD_ST_CONNECTING);
		}
		else{
			perror("epoll_ctl(add)");
			thread_goto_state(t, THREAD_ST_ERROR);
		}
	}
	else{
		thread_goto_state(t, THREAD_ST_ERROR);
	}
}

static void sf_connecting(struct thread* t){
	t->write_pos = 0;
	thread_goto_state(t, THREAD_ST_HEADER_SENDING);
}

static void sf_header_sending(struct thread* t){
	int n = write(
		t->peerfd,
		t->req->req + t->write_pos,
		t->req->req_len - t->write_pos);
	++t->stat.txn;
	t->stat.tx += n;

	if(n == t->req->req_len - t->write_pos){
#ifdef IGNORE_RESPONSE
		// if we don't care about resp
		thread_goto_state(r, THREAD_ST_DONE);
#else
		struct epoll_event ev;
		ev.data.ptr = (void*)t;
		ev.events = EPOLLIN | EPOLLET;
		if(0 == epoll_ctl(t->epollfd, EPOLL_CTL_MOD, t->peerfd, &ev)){
			thread_set_state(t, THREAD_ST_HEADER_READING);
		}
		else{
			perror("epoll_ctl(mod)");
			thread_goto_state(t, THREAD_ST_ERROR);
		}
#endif
	}
	else if(n < 0){
		perror("write");
		t->stat.tx -= n; // undo
		thread_goto_state(t, THREAD_ST_ERROR);
	}
}

static void sf_header_reading(struct thread* t){
	int n;
	do{
		n = read(t->peerfd,
			 t->resp + t->read_pos,
			 LEN(t->resp) - t->read_pos - 1);
		if(n >= 0){
			char *a;

			t->read_pos += n;
			t->resp[t->read_pos] = '\0';

			++t->stat.rxn;
			t->stat.rx += n;

			print_dbg("Reply[%d]", t->read_pos);
			t->read_pos = 0;
			a = strrchr(t->resp, '/');
			if(a){
				print_dbg("Reply[%d]: %s", t->resp_len, a);
				if(0 == memcmp(a-1, "</html>", strlen("</html>"))){
					thread_goto_state(t, THREAD_ST_DONE);
					break;
				}
			}
		}
		else if(errno != EAGAIN && errno != EWOULDBLOCK){
			if(errno != EINPROGRESS){
				perror("read");
			}
			thread_goto_state(t, THREAD_ST_ERROR);
		}
	}while(n > 0);
}

static void sf_done(struct thread* t){
	++t->stat.count;
	epoll_ctl(t->epollfd, EPOLL_CTL_DEL, t->peerfd, NULL);
	close(t->peerfd);
	thread_goto_state(t, THREAD_ST_BEGIN);
}

static void sf_error(struct thread* t){
	++t->stat.error;
	thread_goto_state(t, THREAD_ST_DONE);
}
