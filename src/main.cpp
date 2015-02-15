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
#include "poller.h"
#include "request.h"

int main(int argc, char* argv[])
{
	struct poller p;
	int threads;
	struct request req;
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

	threads = atoi(argv[1]);
	assert(threads > 0);

	inet_pton(AF_INET, argv[2], &req.dst.sin_addr);
	req.dst.sin_family = AF_INET;
	req.dst.sin_port = htons(atoi(argv[3]));
	req.req_len =
		snprintf(req.req, LEN(req.req),
			 "GET %s HTTP/1.1\r\n"
			 "Host: www.bloomberg.com\r\n"
			 "Connection: close\r\n"
			 "User-Agent: http-bomb 1.0\r\n"
			 "\r\n",
			 argv[4]);
	req.timeout_ms = 100000;
	p.reqs.push_back(req);

	printf("Will use %d threads to generate traffic to http://%s:%s%s\n",
	       threads, argv[2], argv[3], argv[4]);

	for(i=0; i<threads; ++i){
		struct thread th;
		p.threads.push_back(th);
	}

	poller_run(&p, 10, 0);

	return 0;
}
