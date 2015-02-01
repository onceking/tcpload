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

char const* REQST_STRS[] = {"SLEEP", "BEGIN", "CONNECTING", "CONNECTED", 
			    "SENDING_HEADER", "SENT_HEADER",
			    "SENDING_FILE", "SENT_FILE",
			    "READING_RESP", "READ_RESP",
			    "ADV_CONNECTING", "ADV_CONNECTED",
			    "ADV_SENDING", "ADV_SENT",
			    "ADC_CONNECTING", "ADC_CONNECTED",
			    "ADC_SENDING", "ADC_SENT",
			    "END"};
const char* AD_IP = "128.238.63.11";
struct sockaddr_in AD_SOCKADDR;

int main(int argc, char* argv[]) 
{
	struct request* reqs[100];
	int reqlen = 0;

	struct file* files[100];
	int filelen = 0;
	int threads;
	struct sockaddr_in sockaddr;

	int epollfd;
	struct epoll_event events[LEN(reqs)];

	int i;

	if (argc <= 5) {
		printf("Usage: %s Threads IP PORT PATH img1 img2 ...\n"
		       " Threads: number of concurrent requests to make.\n"
		       " IP:      IP address of the webserver. (not hostname)\n"
		       " PORT:    Port of the webserver. (not C++/C server)\n"
		       " PATH:    path to the php page handling the upload form. (always start with /)\n"
		       " imgN:    image file names (up to 100).\n"
		       "Example: %s 2 128.238.63.221 80 /polyflickr/1/upload.php 1.jpg\n"
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

	printf("Will use %d threads to generate traffic to "
	       "http://%s:%s%s", 
	       argv[1], argv[2], argv[3], argv[4]);

	threads = atoi(argv[1]);
	assert(threads > 0 && threads < 100);
	inet_pton(AF_INET, argv[2], &(sockaddr.sin_addr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(atoi(argv[3]));

	inet_pton(AF_INET, AD_IP, &(AD_SOCKADDR.sin_addr));
	AD_SOCKADDR.sin_family = AF_INET;
	AD_SOCKADDR.sin_port = htons(80);

	for(i=0; i<threads; ++i)
	{
		reqs[reqlen] = request_create(argv[4], &sockaddr);
		assert(reqs[reqlen]);
		++reqlen;
	}
	
	for(i=5; i<argc; ++i)
	{
		files[filelen] = file_create(argv[i]);
		if(NULL != files[filelen])
		{
			++filelen;
			print_dbg("Add %s to file list.", argv[i]);
		}
		else
		{
			print_dbg("Failed adding %s to file list.", 
				  argv[i]);
		}
	}
	assert(filelen > 0);


	srand(time(NULL));


	while(1)
	{
		for(i=0; i<reqlen; ++i)
			request_wakeup(reqs[i], epollfd);

		int rdylen = epoll_wait(epollfd, events, LEN(reqs), 100); 
		for(i=0; i<rdylen; ++i)
		{
			struct request* r = (struct request*)(events[i].data.ptr);
			request_process(r, epollfd, files[rand()%filelen]);
		}

		for(i=0; i<reqlen; ++i)
			request_cancel_stale(reqs[i], epollfd, 1000*1000);
		
		// file_set_name(f, rand()%('Z'-'A')+'a');
	}

	return 0;
}
