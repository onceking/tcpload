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
	uint64_t error;
};

void stats_add(struct stats* a, struct stats const* b){
	a->tx    += b->tx;
	a->txn   += b->txn;
	a->rx    += b->rx;
	a->rxn   += b->rxn;
	a->count += b->count;
	a->error += b->error;
}

void stats_clear(struct stats* s){
	memset(s, 0, sizeof(struct stats));
}
#endif
