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

static void thread_set_state(struct thread*, int);
static void thread_goto_state(struct thread*, int);

static void (*state_fn)(struct thread*, struct epoll_event const*);

char const* thread_state_string(int st){
	static char const* strs[] = {
		"BEGIN",
		"SENDING_DNS",
		"READING_DNS",
		"CONNECTING",
		"CONNECTED",
		"SENDING_HEADER",
		"READING_HEADER",
		"READING_BODY",
		"DONE",
		"ERROR"
	};

	asset(st < THREAD_ST_COUNT);

	return strs[st];
}

void thread_set_request(struct thread* t, struct request* r){
	assert(t->state == THREAD_ST_BEGIN);
	t->req = r;
}

void thread_start(struct thread* t){
	assert(t->state == THREAD_ST_BEGIN);
	thread_process(t, NULL);
}

void thread_process(struct thread*, struct epoll_event const*){
}

void request_housekeep(struct thread* t){
	if(time_elasped(t->state_time) > t->req->timeout_ms){
		thread_goto_state(r, THREAD_ST_ERROR);
	}
}

static void thread_set_state(struct thread* t, int s){
	print_dbg("%p: advanced from [%d]%s to [%d]%s. ",
		  t,
		  t->state, thread_state_string(t->state),
		  s, thread_state_string(s));
	t->state = s;
	gettimeofday(r->state_time);
}

static void request_goto_state(struct thread* t, int s){
	thread_set_state(t, s);
	request_process(r, NULL);
}

static void sf_begin(struct thread* t, struct epoll_event* ev){
	t->peerfd = nonblock_connect(&t->req->dst);
	if(t->peerfd > 0){
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

static void sf_connecting(struct thread* t, struct epoll_event* ev){
	r->writepos = 0;
	thread_goto_state(t, THREAD_ST_HEADER_SENDING);
}

static void sf_header_sending(struct thread* t, struct epoll_event* ev){
	switch(nonblock_write(r->peerfd, r->header,
				      &(r->writepos), r->header_len,
				      &r->stat)){
		case WRITE_DONE:{
#ifdef IGNORE_RESPONSE
			// if we don't care about resp
			request_goto_state(r, REQST_END, epollfd);
#else
			struct epoll_event ev;
			ev.data.ptr = (void*)r;
			ev.events = EPOLLIN | EPOLLET;
			if(0 == epoll_ctl(epollfd, EPOLL_CTL_MOD, r->peerfd, &ev)){
				request_set_state(r, REQST_HEADER_READING);
			}
			else{
				print_dbg("epoll_ctl(mod): %s", strerror(errno));
				request_set_state(r, REQST_END);
			}
#endif
		}
			break;
		case WRITE_PARTIAL:
			break;
		case WRITE_FAIL:
			request_goto_state(r, REQST_END, epollfd);
			break;
		}
		break;

	case REQST_HEADER_READING:
		if(r->resp_len >= LEN(r->resp)){
			print_dbg("Response [%d>=%lu] too long.",
				  r->resp_len, LEN(r->resp));
			request_goto_state(r, REQST_END, epollfd);
		}
		else{
			int n;
			do{
				n = read(r->peerfd, r->resp + r->resp_len,
					 LEN(r->resp) - r->resp_len - 1);
				if(n > 0){
					char *a;
					// fwrite(r->resp + r->resp_len, n, 1, stdout);
					r->resp_len += n;
					++r->stat.rxn;
					r->stat.rx += n;
					r->resp[r->resp_len] = '\0';
					// search for key words!!

					// tmp = r->resp[20];
					// r->resp[20] = '\0';
					print_dbg("Reply[%d])", r->resp_len);
					// r->resp[20] = tmp;
					r->resp_len = 0;
					a = strrchr(r->resp, '/');
					if(a){
						print_dbg("Reply[%d]: %s",
							  r->resp_len, a);
						if(0 == memcmp(a-1, "</html>",
							       strlen("</html>"))){
							request_goto_state(
								r, REQST_END, epollfd);
							break;
						}
					}

				}
				else if(errno != EAGAIN && errno != EWOULDBLOCK){
					if(errno != EINPROGRESS){
						perror("read");
					}
					request_goto_state(r, REQST_END, epollfd);
				}
			}while(n > 0);
		}
		break;
	case REQST_END:
		++r->stat.count;
		epoll_ctl(epollfd, EPOLL_CTL_DEL, r->peerfd, NULL);
		close(r->peerfd);
		// fall through
	}

void request_connect(struct request* r, struct sockaddr_in const* sa, int epollfd,
		     int s_ok, int s_fail)
{
}

void request_process(struct request* r, struct epoll_event const* ev, int epollfd)
{
}
