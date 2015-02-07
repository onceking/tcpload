#ifndef STATS_H
#define STATS_H

#include <time.h>

struct stats{
	time_t beg;
	uint64_t tx;
	uint64_t txn;
	uint64_t rx;
	uint64_t rxn;
	uint64_t count;
};

#endif
