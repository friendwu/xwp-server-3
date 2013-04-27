#include <string.h>
#include "typedef.h"
#include "array.h"

array_t* array_create(pool_t* pool, int nalloc)
{
	assert(pool != NULL && nalloc > 0);

	array_t* thiz = pool_calloc(pool, sizeof(array_t) + nalloc * sizeof(void*));
	if(thiz == NULL) return NULL;

	thiz->elts = (void** )((char* )thiz + sizeof(array_t)); 

	thiz->count = 0;
	thiz->nalloc = nalloc;
	thiz->pool = pool;

	return thiz;
}

//TODO put the data alloc operation in it.
int array_push(array_t* thiz, void* data)
{
	assert(thiz != NULL);

	if(thiz->count == thiz->nalloc)
	{
		void** new  = (void** ) pool_alloc(thiz->pool, (thiz->nalloc + thiz->nalloc / 2) * sizeof(void*));

		if(new == NULL) return 0;

		memcpy(new, thiz->elts, thiz->nalloc * sizeof(void*));

		thiz->elts = new;
		thiz->nalloc += thiz->nalloc / 2;
	}
	
	thiz->elts[thiz->count] = data;	
	thiz->count += 1;

	return 1;
}

/*
int array_reset(array_t* thiz)
{
	assert(thiz != NULL);

	thiz->count = 0;

	return 1;
}
*/
