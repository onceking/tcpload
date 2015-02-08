#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/epoll.h>
#include <arpa/inet.h>

#include "req.h"
#include "util.h"
#include "stats.h"

#define PERIOD (1./(1 << 20))

struct request
{
	struct sockaddr_in dst;
	char ipstr[INET6_ADDRSTRLEN];
	int peerfd;

	char* path;


	long writepos;

	char header[4096];
	long header_len;

	char resp[1<<20]; // we are only checking status line now
	int resp_len;

	int state;
	struct timeval state_times[REQST_END+1]; // track timeout
	struct stats stat;
};

static void request_set_state(struct request* r, int s);
struct request* request_create(char const* path, struct sockaddr_in const* dst)
{
	struct request* r = (struct request*)calloc(1, sizeof(struct request));
	if(NULL != r)
	{
		memcpy(&(r->dst), dst, sizeof(r->dst));
		inet_ntop(AF_INET, &(dst->sin_addr), r->ipstr, LEN(r->ipstr));

		r->path = (char*)malloc(strlen(path) + 1);
		memcpy(r->path, path, strlen(path) + 1);

		r->header_len =
			snprintf(r->header, LEN(r->header),
				 "GET %s HTTP/1.1\r\n"
				 "Host: www.bloomberg.com\r\n"
				 "Connection: close\r\n"
				 "User-Agent: http-bomb 1.0\r\n"
				 "\r\n",
				 r->path);
		assert(r->header_len < LEN(r->header));

		request_set_state(r, REQST_BEGIN);
	}

	return r;
}

void request_destroy(struct request* r)
{
	free(r);
}

static void request_set_state(struct request* r, int s){
	print_dbg("%p: advanced from [%d]%s to [%d]%s. ",
		  r,
		  r->state, REQST_STRS[r->state],
		  s, REQST_STRS[s]);
	r->state = s;
	gettimeofday(r->state_times + r->state, NULL);
}
static void request_goto_state(struct request* r, int s, int epollfd)
{
	request_set_state(r, s);
	request_process(r, NULL, epollfd);
}

void request_connect(struct request* r, struct sockaddr_in const* sa, int epollfd,
		     int s_ok, int s_fail)
{
	r->peerfd = nonblock_connect(sa);
	if(r->peerfd > 0)
	{
		struct epoll_event ev;
		ev.events = EPOLLOUT | EPOLLHUP;
		ev.data.ptr = (void*)r;
		if(0 == epoll_ctl(epollfd, EPOLL_CTL_ADD, r->peerfd, &ev))
		{
			request_set_state(r, s_ok);
		}
		else
		{
			print_dbg("epoll_ctl(add): %s", strerror(errno));
			request_set_state(r, s_fail);
		}
	}
	else
	{
		request_set_state(r, s_fail);
	}
}

void request_cancel_stale(struct request* r, int epollfd, int timeout)
{
}
void request_housekeep(struct request* r, int epollfd)
{
	if(request_current_state(r) == REQST_BEGIN ||
	   request_current_state(r) == REQST_END){
		request_process(r, NULL, epollfd);
	}
	else if(time_elasped(request_state_time(r, request_current_state(r))) > 10){
		request_set_state(r, REQST_END);
		request_process(r, NULL, epollfd);
	}
}

void request_process(struct request* r, struct epoll_event const* ev, int epollfd)
{
	int state = request_current_state(r);
	switch(state)
	{
	case REQST_END:
		++r->stat.count;
		epoll_ctl(epollfd, EPOLL_CTL_DEL, r->peerfd, NULL);
		close(r->peerfd);
		// fall through
	case REQST_BEGIN:
		request_connect(r, &(r->dst), epollfd, REQST_CONNECTING, REQST_END);
		break;

	case REQST_CONNECTING: // TODO: noop?
	case REQST_CONNECTED:
		r->writepos = 0;
		// fall through!!!
	case REQST_HEADER_SENDING:
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
						perror("read: %s", strerror(errno));
					}
					request_goto_state(r, REQST_END, epollfd);
				}
			}while(n > 0);
		}
		break;
	}
}

/* int request_fd(struct request const* r) */
/* { */
/* 	return r->peerfd; */
/* } */
struct sockaddr_in const* request_dst(struct request const* r)
{
	return &(r->dst);
}


struct stats const* request_stat(struct request const* r)
{
	return &(r->stat);
}
int request_current_state(struct request const* r)
{
	return r->state;
}
struct timeval const* request_state_time(struct request const* r, int state)
{
	return r->state_times + state;
}
