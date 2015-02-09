#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <arpa/inet.h>
#include <assert.h>

#include "util.h"

int nonblock_connect(struct sockaddr_in const* dst){
	int soc = -1;
	long sockopt;
	char ip[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET, &(dst->sin_addr), ip, sizeof(ip)/sizeof(ip[0]));
	// @todo comment out
	print_dbg("Connecting to %s:%d", ip, ntohs(dst->sin_port));

	soc = socket( AF_INET, SOCK_STREAM, 0);
	if(soc < 0)
	{
     		print_dbg("socket: %s", strerror(errno));
		return -1;
	}

	if((sockopt = fcntl(soc, F_GETFL, NULL)) < 0)
	{
     		print_dbg("Error fcntl(..., F_GETFL): %s", strerror(errno));
		close(soc);
     		return -1;
	}
	sockopt |= O_NONBLOCK;
	if(fcntl(soc, F_SETFL, sockopt) < 0)
	{
     		print_dbg("Error fcntl(..., F_SETFL): %s", strerror(errno));
		close(soc);
    		return -1;
	}

	if(0 == connect(soc, (struct sockaddr*)dst, sizeof(*dst)))
		return soc;

	if(errno != EINPROGRESS){
		print_dbg("connect[%d]: %s", errno, strerror(errno));
		close(soc);
		return -1;
	}

	return soc;
}

int nonblock_write(int fd, char const* buf, long* offset, long len, struct stats* stat){
	int n = write(fd, buf+(*offset), len-(*offset));
	if(n > 0){
		stat->tx += n;
		++stat->txn;
		*offset += n;
		assert(*offset <= len);
		return *offset == len ? WRITE_DONE :WRITE_PARTIAL;
	}

	print_dbg("write[%d]: %s", errno, strerror(errno));
	return WRITE_FAIL;
}

int time_elasped(struct timeval const* beg){
	struct timeval end;
	gettimeofday(&end, NULL);
	return (end.tv_sec - beg->tv_sec)*1000 +
		(end.tv_usec - beg->tv_usec)/1000;
}
