#ifndef MODULE_H
#define MODULE_H
#include "pool.h"
#include "typedef.h"
typedef struct module_so_conf_s module_so_conf_t;

#define HTTP_MODULE_PROCESS_DONE 1
#define HTTP_MODULE_PROCESS_UPSTREAM -1

typedef int (*MODULE_PROCESS_FUNC)(module_t* thiz, http_request_t* request, pool_t* pool);

typedef struct module_s
{
	void* parent;

	MODULE_PROCESS_FUNC process;

	char priv[0];
}module_t;

//must hook the destroy handler on the pool cleanup. 
typedef module_t* (*HANDLER_CREATE_FUNC)(vhost_loc_conf_t* parent, array_t* params, pool_t* pool);

static inline int module_process(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);

	if(thiz->process != NULL)
		return thiz->process(thiz, request);
	
	return HTTP_MODULE_PROCESS_FAIL;
}

#endif
