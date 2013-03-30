#ifndef MODULE_H
#define MODULE_H
#include "pool.h"
#include "typedef.h"
typedef struct module_so_conf_s module_so_conf_t;

#define HTTP_MODULE_PROCESS_DONE 1
#define HTTP_MODULE_PROCESS_FAIL 0
#define HTTP_MODULE_PROCESS_UPSTREAM -1

typedef int (*MODULE_PROCESS_FUNC)(module_t* thiz, http_request_t* request, pool_t* pool);

typedef struct module_s
{
	module_so_conf_t* parent;

	MODULE_PROCESS_FUNC process;

	char priv[0];
}module_t;

//TODO should hook the destroy handler to the pool. 
typedef module_t* (*MODULE_CREATE_FUNC)(module_so_conf_t* conf, array_t* params, pool_t* pool);

static inline int module_process(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);

	if(thiz->process != NULL)	
		return thiz->process(thiz, request);
	
	return HTTP_MODULE_PROCESS_FAIL;
}

#endif
