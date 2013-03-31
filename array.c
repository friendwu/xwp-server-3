#include <string.h>
#include "array.h"
#include "pool.h"

int array_init(array_t* thiz, pool_t* pool, int nalloc)
{
	assert(thiz!=NULL && pool!=NULL && size > 0);

	thiz->elts = (void** ) pool_alloc(pool, nalloc * sizeof(void*));
	if(thiz->elts == NULL) return 0;

	thiz->count = 0;
	thiz->nalloc = nalloc;
	thiz->pool = pool;

	return 1;
}

int array_push(array_t* thiz, void* data)
{
	assert(thiz != NULL);
	if(thiz->count == thiz->nalloc)
	{
		void** new  = pool_alloc(thiz->pool, thiz->nalloc + thiz->nalloc / 2);

		if(new == NULL) return 0;

		memcpy(new, thiz->data, thiz->nalloc * sizeof(void*));

		thiz->elts = new;
		thiz->nalloc = thiz->nalloc + thiz->nalloc / 2;
	}
	
	thiz->elts[thiz->count] = data;	
	thiz->count += 1;

	return 1;
}
