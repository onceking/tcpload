#ifndef REQUEST_H
#define REQUEST_H

#include <time.h>
#include <netdb.h>

struct request{
	struct sockaddr_in ns;
	struct sockaddr_in dst;
	time_t ns_expire;

	unsigned timeout_ms;
	char req[4096];
	long req_len;

	struct stats stat;
};

#endif
