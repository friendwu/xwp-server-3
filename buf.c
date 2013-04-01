#include "typedef.h"
#include "buf.h"
#include "pool.h"

int buf_create(buf_t* thiz, pool_t* pool, int size)
{
	assert(thiz!=NULL && pool!=NULL && size>0);

	thiz->start = pool_alloc(pool, size);
	if(thiz->start == NULL) return 0;

	thiz->end = thiz->start + size;
	thiz->pos = thiz->start;
	thiz->last = thiz->start;

	return 1;
}

