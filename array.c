#include <string.h>
#include "typedef.h"
#include "array.h"

int array_init(array_t* thiz, pool_t* pool, int nalloc)
{
	assert(thiz!=NULL && pool!=NULL && nalloc > 0);

	thiz->elts = (void** ) pool_alloc(pool, nalloc * sizeof(void*));
	if(thiz->elts == NULL) return 0;

	thiz->count = 0;
	thiz->nalloc = nalloc;
	thiz->pool = pool;

	return 1;
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
