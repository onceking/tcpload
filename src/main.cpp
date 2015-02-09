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
#include "req.h"

int main(int argc, char* argv[])
{
	struct poller p;
	struct sockaddr_in sockaddr;
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

	p.threads = atoi(argv[1]);
	assert(p.threads > 0);

	inet_pton(AF_INET, argv[2], &(sockaddr.sin_addr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_port = htons(atoi(argv[3]));

	printf("Will use %d threads to generate traffic to http://%s:%s%s\n",
	       p.threads, argv[2], argv[3], argv[4]);

	for(i=0; i<p.threads; ++i)
	{
		struct request r;
		memset(&r, 0, sizeof(r));

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
struct request* r = request_create(argv[4], &sockaddr);
		assert(r);
		p.reqs.push_back(r);
	}

	poller_run(&p, 10, 0);

	return 0;
}
