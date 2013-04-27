#include "typedef.h"
#include "buf.h"
#include "pool.h"

buf_t* buf_create(pool_t* pool, int size)
{
	assert(pool!=NULL && size>=0);

	buf_t* thiz = pool_calloc(pool, sizeof(buf_t) + size);
	if(thiz == NULL) return NULL;

	thiz->start = (char*) thiz + sizeof(buf_t);
	thiz->pos = thiz->start;
	thiz->last = thiz->start;
	thiz->end = thiz->start + size;

	return thiz;
}

