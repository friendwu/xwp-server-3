#ifndef ARRAY_H
#define ARRAY_H
#include "pool.h"

typedef struct array_s array_t;
struct array_s
{
	pool_t* pool;
	void** elts;
	int count;
	int nalloc;
};

array_t* array_create(pool_t* pool, int nalloc);
int array_push(array_t* thiz, void* data);
/*int array_reset(array_t* thiz);*/

#endif
