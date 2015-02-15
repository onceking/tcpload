#ifndef STATS_H
#define STATS_H

#include <time.h>
#include <stdint.h>
struct stats{
	time_t beg;
	uint64_t tx;
	uint64_t txn;
	uint64_t rx;
	uint64_t rxn;
	uint64_t count;
	uint64_t error;
};

void stats_add(struct stats*, struct stats const*);
void stats_clear(struct stats*);
#endif
