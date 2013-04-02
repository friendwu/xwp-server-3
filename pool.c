#include <string.h>
#include <stdlib.h>
#include "pool.h"
#include "typedef.h"

typedef struct pool_node_s
{
	struct pool_node_s* next;
}pool_node_t;

typedef struct pool_cleanup_node_s
{
	void* ctx;
	POOL_CLEANUP_FUNC cleanup;
	struct pool_cleanup_node_s* next;
}pool_cleanup_node_t;

typedef struct pool_s
{
	pool_node_t* head;	

	pool_cleanup_node_t* cleanup_head;
}pool_t;


pool_t* pool_create(int size)
{
	pool_t* thiz = calloc(1, sizeof(pool_t));

	return thiz;
}

void* pool_alloc(pool_t* thiz, int size)
{
	return_val_if_fail(thiz!=NULL && size > 0, NULL);

	char* p = (char* )malloc(size + sizeof(pool_node_t));

	if(p)
	{
		pool_node_t* n = (pool_node_t* )p;
		n->next = thiz->head;
		thiz->head = n;
	}

	return p + sizeof(pool_node_t);
}

void* pool_calloc(pool_t* thiz, int size)
{
	return_val_if_fail(thiz!=NULL && size>0, 0);
	void* p = pool_alloc(thiz, size);

	if(p)
	{
		memset(p, 0, size);
	}

	return p;
}

int pool_add_cleanup(pool_t* thiz, POOL_CLEANUP_FUNC cleanup, void* ctx)
{
	return_val_if_fail(thiz!=NULL && cleanup!=NULL, 0);
	pool_cleanup_node_t* p = malloc(sizeof(pool_cleanup_node_t));	

	if(p)
	{
		p->next = thiz->cleanup_head;
		p->ctx = ctx;
		p->cleanup = cleanup;
		thiz->cleanup_head = p;

		return 1;
	}

	return 0;
}

//TODO cleanup
int pool_reset(pool_t* thiz)
{
	return_val_if_fail(thiz!=NULL, 0);

	return 1;
}

int pool_destroy(pool_t* thiz)
{
	return_val_if_fail(thiz!=NULL, 0);

	pool_cleanup_node_t* c = thiz->cleanup_head;

	while(c != NULL)
	{
		pool_cleanup_node_t* cc = c->next;

		c->cleanup(c->ctx);
		free(c);
		c = cc;
	}

	pool_node_t* n = thiz->head;

	while(n != NULL)
	{
		pool_node_t* nn = n->next;

		free(n);
		n = nn;
	}

	return 1;
}

char* pool_strdup(pool_t* thiz, char* str)
{
	return_val_if_fail(str!=NULL && thiz!=NULL, NULL);

	size_t slen = strlen(str);
	char* p = pool_alloc(thiz, slen + 1);
	if(p == NULL) return NULL;

	memcpy(p, str, slen);
	p[slen] = '\0';

	return p;
}

char* pool_strndup(pool_t* thiz, char* str, int len)
{
	return_val_if_fail(str!=NULL && thiz!=NULL && len>0, NULL);

	char* p = pool_alloc(thiz, len + 1);
	if(p == NULL) return NULL;

	memcpy(p, str, len);
	p[len] = '\0';

	return p;
}
