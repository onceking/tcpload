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

	soc = socket(AF_INET, SOCK_STREAM, 0);
	if(soc < 0){
     		perror("socket");
		return -1;
	}

	if((sockopt = fcntl(soc, F_GETFL, NULL)) < 0){
     		perror("fcntl(F_GETFL)");
		close(soc);
     		return -1;
	}
	sockopt |= O_NONBLOCK;
	if(fcntl(soc, F_SETFL, sockopt) < 0){
     		print_dbg("fcntl(F_SETFL)");
		close(soc);
    		return -1;
	}

	if(0 == connect(soc, (struct sockaddr*)dst, sizeof(*dst))){
		return soc;
	}

	if(errno != EINPROGRESS){
		perror("connect");
		close(soc);
		return -1;
	}

	return soc;
}

int time_elasped(struct timeval const* beg){
	struct timeval end;
	gettimeofday(&end, NULL);
	return (end.tv_sec - beg->tv_sec)*1000 +
		(end.tv_usec - beg->tv_usec)/1000;
}
