#include <stdlib.h>
typedef struct pool_s
{
	pool_node_s* head;	
}pool_t;

typdef struct pool_node_s
{
	struct pool_node_s* next;
}pool_node_t;

pool_t* pool_create(int size)
{
	pool_t* thiz = calloc(1, sizeof(pool_t));	

	return thiz;
}
char* pool_alloc(pool_t* thiz, int size)
{
	char* p = malloc(size + sizeof(pool_node_t));	

	if(p)
	{
		pool_node_t* n = (pool_node_t* )p;
		n->next = thiz->head;
		head = n;
	}

	return p + sizeof(pool_node_t);
}

char* pool_calloc(pool_t* thiz, int size)
{
	char* p = pool_alloc(thiz, size);

	if(p)
	{
		memset(p, 0, size);
	}

	return p;
}

int pool_reset(pool_t* thiz)
{
	return 1;
}

int pool_destroy(pool_t* thiz)
{
	pool_node_t* n = thiz->head;

	while(n != NULL)
	{
		pool_node_t*nn = n->next;

		free(n);

		n = nn;
	}

	return 1;
}
