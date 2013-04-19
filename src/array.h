#ifndef ARRAY_H
#define ARRAY_H
#include "pool.h"

typedef struct array_s
{
	pool_t* pool;
	void** elts;
	int count;
	int nalloc;
}array_t;

int array_init(array_t* array, pool_t* pool, int nalloc);
int array_push(array_t* array, void* data);

#endif 