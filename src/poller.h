#ifndef POLLER_H
#define POLLER_H

#include "req.h"

struct poller;

struct poller* poller_create();

void poller_add(struct request*);

void poller_run(struct poller*, unsigned duration, unsigned trans);
void poller_destroy(struct poller*);

#endif
