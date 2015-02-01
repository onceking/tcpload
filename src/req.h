#ifndef REQ_H
#define REQ_H

#include <netdb.h>

enum {
	REQST_SLEEP=0,               // so we can slow it down.
	REQST_BEGIN,           // init, start connecting
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

struct stat
{
	int transfers;
	int repeat;
};

void request_process(struct request* r, int epollfd);
void request_cancel_stale(struct request* r, int epollfd, int timeout);
void request_wakeup(struct request* r, int epollfd);

struct request* request_create(char const* path, struct sockaddr_in const* dst);
void request_destroy(struct request* r);

struct stat const* request_stat(struct request const* r);
int request_current_state(struct request const* r);
struct timeval const* request_state_time(struct request const* r, int state);



#endif
