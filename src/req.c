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

#define PERIOD (1./(1 << 20))
#define BOUNDARY "192.168.1.61.1000.11309.1299659472.313.1"

struct request
{
	int force_event;

	struct sockaddr_in dst;
	char ipstr[INET6_ADDRSTRLEN];
	int peerfd;

	char* path;


	long writepos;

	char header[4096];
	long header_len;

	char resp[1024]; // we are only checking status line now
	int resp_len;

	int state;
	struct timeval state_times[REQST_END+1]; // track timeout
	struct stat stat;
};

static void request_set_state(struct request* r, int s);
struct request* request_create(char const* path, struct sockaddr_in const* dst)
{
	struct request* r = (struct request*)calloc(1, sizeof(struct request));
	if(NULL != r)
	{
		r->force_event = 0;
		memcpy(&(r->dst), dst, sizeof(r->dst));
		inet_ntop(AF_INET, &(dst->sin_addr), r->ipstr, LEN(r->ipstr));

		r->path = (char*)malloc(strlen(path) + 1);
		memcpy(r->path, path, strlen(path) + 1);

		r->header_len =
			snprintf(r->header, LEN(r->header),
				 "GET %s HTTP/1.1\r\n"
				 "Host: %s:%d\r\n"
				 "Connection: close\r\n"
				 "User-Agent: http-bomb 1.0\r\n"
				 "\r\n",
				 r->path, r->ipstr, ntohs(r->dst.sin_port));
		assert(r->header_len < LEN(r->header));

		request_set_state(r, REQST_SLEEP);
	}

	return r;
}

void request_destroy(struct request* r)
{
	free(r);
}

static int write_and_next(int fd, char const* buf, long* offset, long len,
			  int s_partial, int s_done, int s_fail)
{
	int n = write(fd, buf+(*offset), len-(*offset));
	if(n > 0)
	{
		*offset += n;
		assert(*offset <= len);
		return *offset == len ?s_done :s_partial;
	}

	print_dbg("write: %s", strerror(errno));
	return s_fail;
}

static void request_set_state_event(struct request* r, int s, int force_event)
{
	print_dbg("%p: advanced from [%d]%s to [%d]%s.", r,
		  r->state, REQST_STRS[r->state],
		  s, REQST_STRS[s]);
	r->state = s;
	r->force_event = force_event;
	gettimeofday(r->state_times + r->state, NULL);
}
static void request_set_state(struct request* r, int s)
{
	request_set_state_event(r, s, 0);
}

void request_connect(struct request* r, struct sockaddr_in const* sa, int epollfd,
		     int s_ok, int s_fail)
{
	r->peerfd = nonblock_connect(sa);
	if(r->peerfd > 0)
	{
		struct epoll_event ev;
		ev.events = EPOLLOUT; // EPOLLIN |
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
	if(request_current_state(r) != REQST_SLEEP &&
	   time_elasped(request_state_time(r, request_current_state(r))) > timeout)
	{
		request_set_state(r, REQST_END);
		request_process(r, epollfd);
	}
}
void request_wakeup(struct request* r, int epollfd)
{
	if(r->force_event)
		request_process(r, epollfd);

	if(request_current_state(r) == REQST_SLEEP &&
	   time_elasped(request_state_time(r, REQST_BEGIN)) >= PERIOD)
	{
		request_set_state_event(r, REQST_BEGIN, 1);
	}
}
void request_process(struct request* r, int epollfd)
{
	int state = request_current_state(r);
	r->force_event = 0;
	switch(state)
	{
	case REQST_BEGIN:
		request_connect(r, &(r->dst), epollfd, REQST_CONNECTING, REQST_END);
		break;

	case REQST_CONNECTING:
	case REQST_CONNECTED:
		r->writepos = 0;
		// fall through!!!
	case REQST_HEADER_SENDING:
		request_set_state(r, write_and_next(
					  r->peerfd,
					  r->header, &(r->writepos), r->header_len,
					  REQST_HEADER_SENDING,
					  REQST_HEADER_SENT,
					  REQST_END));
		break;


	case REQST_HEADER_SENT:
#ifdef IGNORE_RESPONSE
		// if we don't care about resp
		request_set_state_event(r, REQST_END, 1);
#else
		request_set_state(r, REQST_READING);
#endif
		break;
	case REQST_READING:
		if(r->resp_len >= LEN(r->resp))
		{
			print_dbg("Response [%d>=%lu] too long.",
				  r->resp_len, LEN(r->resp));
			request_set_state_event(r, REQST_END, 1);
		}
		else
		{
			int n = read(r->peerfd, r->resp + r->resp_len,
				     LEN(r->resp) - r->resp_len);
			if(n > 0)
			{
				char* tmp;
				char *a, *b, *c;
				char d;
				// fwrite(r->resp + r->resp_len, n, 1, stdout);
				r->resp_len += n;
				// search for key words!!

				a = strrchr(r->resp, '\n');
				b = strrchr(r->resp, ' ');
				c = strrchr(r->resp, '>');
				if(b > a)
					a = b;
				if(c > a)
					a = c;
				d = *(a+1);
				*(a+1) = '\0';
				print_dbg("Reply: %s", r->resp);
				*(a+1) = d;
				memmove(r->resp, a+1, r->resp_len - (a - r->resp + 1));
				r->resp_len -= (a - r->resp + 1);
			}
			else
			{
				request_set_state_event(r, REQST_END, 1);
			}
		}
		break;

	case REQST_READ:
		close(r->peerfd);
		request_set_state_event(r, REQST_END, 1);
		break;

	case REQST_END:
		print_dbg("%p: Trans: %d   Repeat: %d",
			  r, r->stat.transfers, r->stat.repeat);
		epoll_ctl(epollfd, EPOLL_CTL_DEL, r->peerfd, NULL);
		close(r->peerfd);
		++r->stat.repeat;
		request_set_state(r, REQST_SLEEP); // dead end
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


struct stat const* request_stat(struct request const* r)
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
