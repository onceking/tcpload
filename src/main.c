/**
 * @file   main.c
 * @author Sai <swang02@students.poly.edu>
 * @date   Tue Mar  8 19:38:13 2011
 *
 * @brief  Traffic generator for poly-flickr..
 *         Use nonblock I/O to increase perf.
 *
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include "util.h"
#include "req.h"

char const* REQST_STRS[] = {"BEGIN", "CONNECTING", "CONNECTED",
			    "SENDING_HEADER", "SENT_HEADER",
			    "READING_RESP", "READ_RESP",
			    "END"};

int main(int argc, char* argv[])
{
	struct request* reqs[1024];
	int reqlen = 0;

	int threads;
	struct sockaddr_in sockaddr;

	int epollfd;
	struct epoll_event events[LEN(reqs)];

	struct {
		time_t time_begin;
		unsigned transfers;
	} stats;

	int i;

	if (argc <= 4) {
		printf("Usage: %s Threads IP PORT PATH\n"
		       " Threads: number of concurrent requests to make.\n"
		       " IP:      IP address of the webserver. (not hostname)\n"
		       " PORT:    Port of the webserver. (not C++/C server)\n"
		       " PATH:    path to the php page handling the upload form. (always start with /)\n"
		       "Example: %s 2 128.238.63.221 80 /polyflickr/1/upload.php\n"
		       "         http://128.238.63.221:80/polyflickr/1/upload.php is the PHP page.\n",
		       argv[0], argv[0]);

		return argc == 1 ?0 :-1;
	}

	signal(SIGPIPE, SIG_IGN); // ignore broken pipe.

	epollfd = epoll_create(LEN(reqs)+10);
	if (epollfd == -1)
	{
		print_dbg("epoll_create: %s", strerror(errno));
		exit(EXIT_FAILURE);
	}

	threads = atoi(argv[1]);
	assert(threads > 0 && threads < LEN(reqs));
	inet_pton(AF_INET, argv[2], &(sockaddr.sin_addr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(atoi(argv[3]));

	printf("Will use %d threads to generate traffic to http://%s:%s%s\n",
	       threads, argv[2], argv[3], argv[4]);

	for(i=0; i<threads; ++i)
	{
		reqs[reqlen] = request_create(argv[4], &sockaddr);
		assert(reqs[reqlen]);
		request_start(reqs[reqlen], epollfd);
		++reqlen;
	}

	time(&stats.time_begin);
	stats.transfers = 0;

	while(1)
	{
		struct timeval poll_beg;
		gettimeofday(&poll_beg, NULL);
		do{
			int rdylen = epoll_wait(epollfd, events, LEN(reqs), 110);
			print_dbg("%d events ready", rdylen);
			for(i=0; i<rdylen; ++i)
			{
				struct request* r = (struct request*)(events[i].data.ptr);
				request_process(r, epollfd);
				if(request_current_state(r) != REQST_END){
					++stats.transfers;
					if(stats.transfers % 1000 == 1){
						int t = time(NULL) - stats.time_begin;
						printf("%d transfers/%d seconds, %.2f/s\n",
						       stats.transfers, t,
						       (double)stats.transfers/t);

					}
				}
			}




		}while(time_elasped(&poll_beg) < 0.1);

		for(i=0; i<reqlen; ++i)
			request_cancel_stale(reqs[i], epollfd, 1000*1000);
	}

	return 0;
}
