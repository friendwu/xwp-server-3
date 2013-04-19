#ifndef UPSTREAM_H
#define UPSTREAM_H
typedef void (*UPSTREAM_PROCESS_FUNC)(upstream_t* thiz, http_request_t* request);
typedef void (*UPSTREAM_ABORT_FUNC)(upstream_t* thiz);

typedef struct upstream_s
{
	UPSTREAM_PROCESS_FUNC process;
	UPSTREAM_ABORT_FUNC abort;

	module_t* module;

	char priv[0];
}upstream_t;

static inline void upstream_process(upstream_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);

	if(thiz->process != NULL)
		thiz->process(thiz, request);
	
	request->response.status = HTTP_STATUS_BAD_GATEWAY;
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
