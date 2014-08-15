#ifndef MODULE_H
#define MODULE_H
#include "pool.h"
#include "http.h"
#include "xml_tree.h"
#include "typedef.h"

#define HTTP_MODULE_PROCESS_DONE 1
#define HTTP_MODULE_PROCESS_UPSTREAM -1

typedef void* (*MODULE_CONF_PARSE_FUNC)(pool_t* pool, const XmlNode* node);
typedef int (*MODULE_GET_INFO_FUNC)(module_so_conf_t* so_conf, pool_t* pool);
typedef int (*MODULE_PROCESS_FUNC)(module_t* thiz, http_request_t* request);
typedef module_t* (*MODULE_CREATE_FUNC)(void* ctx, XmlNode* xml_conf_node, pool_t* pool);//must hook the destroy handler on the pool cleanup.

struct module_s
{
	//for handle, the ctx means vhost_loc_conf_t*
	//void* ctx;
	MODULE_PROCESS_FUNC process;

	char priv[0];
};

static inline int module_process(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);

	if(thiz->process != NULL)
		return thiz->process(thiz, request);

	return HTTP_MODULE_PROCESS_DONE;
}

#endif
