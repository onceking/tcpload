#include <string.h>

#include "stats.h"

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
