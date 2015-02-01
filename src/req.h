#ifndef REQ_H
#define REQ_H

#include <netdb.h>

// pad the protocol header at the beginning of content, and include in size.
struct file;
struct file* file_create(char const* filename);

// replace 1 byte of file name
void file_set_name(struct file* f, char c);
void file_destroy(struct file* f);
char* const file_content(struct file const* f);
long file_length(struct file const* f);

enum {
	REQST_SLEEP=0,               // so we can slow it down.
	REQST_BEGIN,           // init, start connecting
	REQST_CONNECTING,        // wait       
	REQST_CONNECTED,         // start sending request
	REQST_HEADER_SENDING,    // wait
	REQST_HEADER_SENT,       // start sending file
	REQST_FILE_SENDING,      // wait
	REQST_FILE_SENT,         // start checking
	REQST_READING,           // 
	REQST_READ,

	REQST_ADV_CONNECTING,
	REQST_ADV_CONNECTED, 
	REQST_ADV_SENDING,
	REQST_ADV_SENT,

	REQST_ADC_CONNECTING,
	REQST_ADC_CONNECTED, 
	REQST_ADC_SENDING,
	REQST_ADC_SENT,

	REQST_END                // finished
};
extern char const* REQST_STRS[];
struct request;

struct stat
{
	int clicks;
	int views;
	int transfers;
	int repeat;
};

void request_process(struct request* r, int epollfd, struct file const* file);
void request_cancel_stale(struct request* r, int epollfd, int timeout);
void request_wakeup(struct request* r, int epollfd);

struct request* request_create(char const* path, struct sockaddr_in const* dst);
void request_destroy(struct request* r);

struct stat const* request_stat(struct request const* r);
int request_current_state(struct request const* r);
struct timeval const* request_state_time(struct request const* r, int state);



#endif
