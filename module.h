#ifndef MODULE_H
#define MODULE_H
#include "typedef.h"

#define HTTP_MODULE_PROCESS_DONE 1
#define HTTP_MODULE_PROCESS_FAIL 0
#define HTTP_MODULE_PROCESS_UPSTREAM -1

typedef module_t* (*MODULE_CREATE_FUNC)(server_t* server, module_so_conf_t* conf, module_param_t* params, size_t param_nr);

typedef struct _module
{
	module_so_conf_t* parent;

	MODULE_PROCESS_FUNC process;
	MODULE_DESTROY_FUNC destroy;

	char priv[0];
}module_t;

static inline int module_process(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);

	if(thiz->process != NULL)	
		return thiz->process(thiz, request);
}

static inline int module_destroy(module_t* thiz)
{
	assert(thiz != NULL);

	if(thiz->destroy != NULL)
		thiz->destroy(thiz);
}

#endif
