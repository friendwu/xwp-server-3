#ifndef MODULE_H
#define MODULE_H
#include "pool.h"
#include "http.h"
#include "conf.h"

#define HTTP_MODULE_PROCESS_DONE 1
#define HTTP_MODULE_PROCESS_UPSTREAM -1
typedef struct module_so_conf_s module_so_conf_t;
typedef struct vhost_loc_conf_s vhost_loc_conf_t;
typedef struct module_s module_t;

typedef int (*MODULE_GET_INFO_FUNC)(module_so_conf_t* so_conf, pool_t* pool);
typedef module_t* (*MODULE_CREATE_FUNC)(void* parent, array_t* params, pool_t* pool);//must hook the destroy handler on the pool cleanup. 
typedef int (*MODULE_PROCESS_FUNC)(module_t* thiz, http_request_t* request);

typedef struct module_s
{
	void* parent;

	MODULE_PROCESS_FUNC process;

	char priv[0];
}module_t;

static inline int module_process(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);

	if(thiz->process != NULL)
		return thiz->process(thiz, request);
	
	return HTTP_MODULE_PROCESS_DONE;
}

#endif
