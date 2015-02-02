#ifndef STATS_H
#define STATS_H

#include <time.h>

struct stats{
	time_t beg;
	unsigned tx;
	unsigned rx;
	unsigned count;
};

#endif
