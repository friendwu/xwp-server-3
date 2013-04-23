#ifndef BUF_H
#define BUF_H
#include "pool.h"
typedef struct buf_s
{
	char* start;
	char* end;
	char* pos;
	char* last;
}buf_t;

typedef struct chain_s
{
	buf_t buf;
	struct chain_s* next;
}chain_t;

int buf_create(buf_t* thiz, pool_t* pool, int size);

#endif
