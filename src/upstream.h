#ifndef UPSTREAM_H
#define UPSTREAM_H
#include <assert.h>
#include <stdio.h>
#include "http.h"
#include "module.h"

typedef void (*UPSTREAM_PROCESS_FUNC)(upstream_t* thiz);
typedef int (*UPSTREAM_ABORT_FUNC)(upstream_t* thiz);

struct upstream_s
{
	UPSTREAM_PROCESS_FUNC process;
	UPSTREAM_ABORT_FUNC abort;

	module_t* module;
	http_request_t* request;

	char priv[0];
};

static inline void upstream_process(upstream_t* thiz)
{
	assert(thiz!=NULL);

	if(thiz->process != NULL)
	{
		thiz->process(thiz);
	}
	else
	{
		thiz->request->status = HTTP_STATUS_BAD_GATEWAY;
	}

	return;
}

static inline void upstream_abort(upstream_t* thiz)
{
	assert(thiz!=NULL);

	if(thiz->abort != NULL)
		thiz->abort(thiz);

	return;
}

#endif
