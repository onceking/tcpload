#ifndef REQ_H
#define REQ_H

#include <netdb.h>

enum {
	REQST_BEGIN,             // init, start connecting
	REQST_CONNECTING,        // wait
	REQST_CONNECTED,         // start sending request
	REQST_HEADER_SENDING,    // wait
	REQST_HEADER_SENT,       // start reading resp
	REQST_READING,           //
	REQST_READ,

	REQST_END                // finished
};
extern char const* REQST_STRS[];
struct request;

void request_process(struct request* r, struct epoll_event const*, int epollfd);

void request_cancel_stale(struct request* r, int epollfd, int timeout);
void request_housekeep(struct request* r, int epollfd);

struct request* request_create(char const* path, struct sockaddr_in const* dst);
void request_destroy(struct request* r);

struct stats const* request_stat(struct request const* r);
int request_current_state(struct request const* r);
struct timeval const* request_state_time(struct request const* r, int state);



#endif
