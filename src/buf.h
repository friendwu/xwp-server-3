#ifndef BUF_H
#define BUF_H
#include "pool.h"
typedef struct buf_s buf_t;

struct buf_s
{
	char* start;
	char* end;
	char* pos;
	char* last;
};

/*
typedef struct chain_s
{
	buf_t buf;
	struct chain_s* next;
}chain_t;
*/

buf_t* buf_create(pool_t* pool, int size);

#endif
