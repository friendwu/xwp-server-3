#ifndef UPSTREAM_H
#define UPSTREAM_H
#define HTTP_UPSTREAM_DONE 1
#define HTTP_UPSTREAM_FAIL 0
typedef int (*UPSTREAM_PROCESS_FUNC)(upstream_t* thiz, http_request_t* request);
typedef int (*UPSTREAM_ABORT_FUNC)(upstream_t* thiz);

typedef struct upstream_s
{
	UPSTREAM_PROCESS_FUNC process;
	UPSTREAM_ABORT_FUNC abort;

	module_t* module;

	char priv[0];
}upstream_t;

static inline int upstream_process(upstream_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);

	if(thiz->process != NULL)
		return thiz->process(thiz, request);
	
	return HTTP_UPSTREAM_DONE;
}

static inline int upstream_abort(upstream_t* thiz)
{
	assert(thiz!=NULL);

	if(thiz->abort != NULL)
		return thiz->abort(thiz);
	
	return HTTP_UPSTREAM_DONE;
}

#endif
