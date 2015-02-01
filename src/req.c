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
extern const char* AD_IP;
extern struct sockaddr_in AD_SOCKADDR;

struct file
{
	char* content; // prefetch files
	long size; // not dealing with huge files

	unsigned filename_offset;
};
struct file* file_create(char const* filename)
{
	struct file* r = (struct file*)malloc(sizeof(struct file));
	if(NULL != r)
	{
		FILE* f;
		char const* basename;
		long flen;
		int pfx_len, sfx_len;
		const int MAX_OVERHEAD = 1024;


		r->content = NULL;

		f = fopen(filename, "rb");
		if(f == NULL)
		{
			print_dbg("Cannot open `%s' for read: %s",
				  filename, strerror(errno));
			goto error;
		}

		fseek(f, 0, SEEK_END);
		flen = ftell(f);
		fseek(f, 0, SEEK_SET);
		if(flen <= 0)
		{
			print_dbg("File `%s' size unknown or empty.", filename);
			fclose(f);
			goto error;
		}

		r->content = (char*)malloc(MAX_OVERHEAD + flen);
		if(NULL == r->content)
		{
			fclose(f);
			goto error;
		}

		basename = strrchr(filename, '/');
		if(basename == NULL)
			basename = filename;
		else
			++basename;
		pfx_len = snprintf(r->content, MAX_OVERHEAD,
				   "--"BOUNDARY"\r\n"
				   "Content-Disposition: form-data; name=\"image\"; filename=\"X%s\"\r\n"
				   "Content-Type: image/jpeg\r\n\r\n",
				   basename);
		if(pfx_len >= MAX_OVERHEAD-strlen(BOUNDARY)+ 128)
		{
			print_dbg("File `%s' size unknown or empty.", filename);
			fclose(f);
			goto error;
		}

		if(1 != fread(r->content+pfx_len, flen, 1, f))
		{
			fclose(f);
			print_dbg("Failed reading %ld bytes from file `%s'",
				  flen, filename);
			goto error;
		}
		fclose(f);

		sfx_len = snprintf(r->content+pfx_len+flen,
				   MAX_OVERHEAD - pfx_len,
				   "\r\n--"BOUNDARY"--\r\n");

		r->size = pfx_len + flen + sfx_len;
		r->filename_offset = strlen("--"BOUNDARY"\r\n"
					    "Content-Disposition: form-data; name=\"image\"; filename=\"");
	}

	return r;

error:
	free(r->content);
	free(r);
	return NULL;
}
char* const file_content(struct file const* f)
{
	return f->content;
}
long file_length(struct file const* f)
{
	return f->size;
}


// replace 1 byte of file name
void file_set_name(struct file* f, char c)
{
	f->content[f->filename_offset] = c;
}

void file_destroy(struct file* f)
{
	free(f->content);
	free(f);
}

struct request
{
	int team_id;
	//int idx;

	int force_event;

	struct sockaddr_in dst;
	char ipstr[INET6_ADDRSTRLEN];
	int peerfd;

	char* path;


	long writepos;

	char header[4096];
	long header_len;
	struct file const* file;

	char resp[1024]; // we are only checking status line now
	int resp_len;

	char adv_req[512];
	long adv_reqlen;
	char adc_req[512];
	long adc_reqlen;

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
		r->team_id = -1;
		//r->idx = idx;
		r->force_event = 0;
		memcpy(&(r->dst), dst, sizeof(r->dst));
		inet_ntop(AF_INET, &(dst->sin_addr), r->ipstr, LEN(r->ipstr));
		r->path = (char*)malloc(strlen(path) + 1);
		memcpy(r->path, path, strlen(path) + 1);

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

void request_switch_file(struct request* r, struct file const* f)
{
	r->header_len =
		snprintf(r->header, LEN(r->header),
			 "POST %s HTTP/1.1\r\n"
			 "Content-Length: %ld\r\n"
			 "Host: %s:%d\r\n"
			 "Content-Type: multipart/form-data; boundary="BOUNDARY"\r\n"
			 "Connection: close\r\n"
			 "User-Agent: Internet Explorer 1.0\r\n"
			 "\r\n",
			 r->path, file_length(f), r->ipstr, ntohs(r->dst.sin_port));

	if(r->header_len >= LEN(r->header))
	{
		print_dbg("head buffer too small..");
		r->file = NULL;
	}
	else
	{
		r->file = f;
	}
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
		request_process(r, epollfd, NULL);
	}
}
void request_wakeup(struct request* r, int epollfd)
{
	if(r->force_event)
		request_process(r, epollfd, NULL);

	if(request_current_state(r) == REQST_SLEEP &&
	   time_elasped(request_state_time(r, REQST_BEGIN)) >= PERIOD)
	{
		request_set_state_event(r, REQST_BEGIN, 1);
	}
}
void request_process(struct request* r, int epollfd, struct file const* file)
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
		request_switch_file(r, file);
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
		if(NULL == r->file)
		{
			request_set_state_event(r, REQST_END, 1);
			break;
		}
		r->writepos = 0;
		// fall through!!!
	case REQST_FILE_SENDING:
		request_set_state(r, write_and_next(
					  r->peerfd,
					  file_content(r->file),
					  &(r->writepos), file_length(r->file),
					  REQST_FILE_SENDING,
					  REQST_FILE_SENT,
					  REQST_END));
		break;

	case REQST_FILE_SENT:
	{
		struct epoll_event ev;
		ev.events = EPOLLIN;
		ev.data.ptr = (void*)r;
		++r->stat.transfers;
		if(0 == epoll_ctl(epollfd, EPOLL_CTL_MOD, r->peerfd, &ev))
		{
			r->resp_len = 0;
			r->team_id = -1;
			request_set_state(r, REQST_READING);
		}
		else
		{
			print_dbg("epoll_ctl(mod): %s", strerror(errno));
			request_set_state_event(r, REQST_END, 1);
		}
		break;
	}

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
				tmp = strstr(r->resp,
					     "http://ad.game.pdc.poly.edu"
					     "/cgi-bin/ad.fcgi?team_id=");
				if(tmp)
				{
					char *end = tmp +
						strlen("http://ad.game.pdc.poly.edu"
						       "/cgi-bin/ad.fcgi?team_id=");
					if(isdigit(end[0]))
					{
						if(isdigit(end[1]))
							end[2] = '\0';
						else
							end[1] = '\0';;

						r->team_id = atoi(end);
						print_dbg("%p: found ads for team %d",
							  r, r->team_id);
						request_set_state_event(r, REQST_READ, 1);
					}
				}
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
		if(r->team_id >= 0)
		{
			r->adv_reqlen =
				snprintf(r->adv_req, LEN(r->adv_req),
					 "GET /cgi-bin/ad.fcgi?team_id=%d HTTP/1.1\r\n"
					 "Host: %s:80\r\n"
					 "User-Agent: Polyflickr Traffic Generator v1\r\n"
					 "\r\n",
					 r->team_id, AD_IP);

			r->adc_reqlen =
				snprintf(r->adc_req, LEN(r->adv_req),
					 "GET /cgi-bin/ad.fcgi?team_id=%d&ad_id=2 HTTP/1.1\r\n"
					 "Host: %s:80\r\n"
					 "User-Agent: Polyflickr Traffic Generator v1\r\n"
					 "\r\n",
					 r->team_id, AD_IP);

			request_connect(r, &AD_SOCKADDR, epollfd,
					REQST_ADV_CONNECTING, REQST_END);
		}
		else
		{
			request_set_state_event(r, REQST_END, 1);
		}
		break;

	case REQST_ADV_CONNECTING:
	case REQST_ADV_CONNECTED:
		r->writepos = 0;
		// fall through!!!
	case REQST_ADV_SENDING:
		request_set_state(r, write_and_next(
					  r->peerfd,
					  r->adv_req, &(r->writepos), r->adv_reqlen,
					  REQST_ADV_SENDING,
					  REQST_ADV_SENT,
					  REQST_END));
		break;

	case REQST_ADV_SENT:
		++r->stat.views;
		++r->stat.transfers;
		close(r->peerfd);
		request_connect(r, &AD_SOCKADDR, epollfd,
				REQST_ADC_CONNECTING, REQST_END);
		break;


	case REQST_ADC_CONNECTING:
	case REQST_ADC_CONNECTED:
		r->writepos = 0;
		// fall through!!!
	case REQST_ADC_SENDING:
		request_set_state(r, write_and_next(
					  r->peerfd,
					  r->adc_req, &(r->writepos), r->adc_reqlen,
					  REQST_ADC_SENDING,
					  REQST_ADC_SENT,
					  REQST_END));
		break;

	case REQST_ADC_SENT:
		++r->stat.clicks;
		++r->stat.transfers;
		request_set_state_event(r, REQST_END, 1);
		break;

	case REQST_END:
		print_dbg("%p: View: %d  Click: %d  Trans: %d   Repeat: %d",
			  r, r->stat.views, r->stat.clicks,
			  r->stat.transfers, r->stat.repeat);
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
