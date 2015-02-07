#ifndef STATS_H
#define STATS_H

#include <time.h>

struct stats{
	time_t beg;
	unsigned long tx;
	unsigned long rx;
	unsigned count;
};

#endif
