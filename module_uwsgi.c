#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "typedef.h"
#include "http_parse.h"
#include "module.h"
#include "upstream.h"

typedef struct module_uwsgi_priv_s 
{
	str_t ip;
	int port;

	array_t pass_params;
}module_uwsgi_priv_t;

typedef struct upstream_uwsgi_priv_s
{
	int fd;
}upstream_uwsgi_priv_t;

static upstream_t* upstream_uwsgi_create(pool_t* pool, module_t* module)
{
	assert(pool != NULL);

	http_response_t* response = &request->response;
	upstream_t* thiz = pool_alloc(pool, sizeof(upstream_t) + sizeof(upstream_uwsgi_priv_t));

	pool_add_cleanup(pool, upstream_uwsgi_destroy, thiz);

	thiz->process = upstream_uwsgi_process;
	thiz->abort = upstream_uwsgi_abort;
	thiz->module = module;

	//open fd.
	DECL_PRIV(module, module_priv, module_uwsgi_priv_t*);
	DECL_PRIV(thiz, upstream_priv, upstream_uwsgi_priv_t*);

	upstream_priv->fd = connect_remote(module_priv->ip.data, module_priv->port);

	return thiz;
}

static int upstream_uwsgi_process(upstream_t* thiz, http_request_t* request)
{
	//send params to remote.
	//recv response.
	//parse response to request->response.
	http_parse_status_line();
	http_parse_header_line();
	http_parse_content_body();

	return HTTP_UPSTREAM_DONE;
}

static int upstream_uwsgi_abort(upstream_t* thiz)
{
	DECL_PRIV(thiz, priv, upstream_uwsgi_priv_t*);

	if(priv->fd > 0)
	{
		//TODO maybe there will be a mutex problem.
		shutdown(priv->fd);
	}

	return 1;
}

static void upstream_uwsgi_destroy(void* data)
{
	return;
}

static void module_uwsgi_destroy(void* data)
{
	//TODO cleanup 
	return;
}

module_t* module_uwsgi_create(void* parent, array_t* params, pool_t* pool)
{
	module_t* thiz = pool_calloc(pool, sizeof(module_t) + sizeof(module_uwsgi_priv_t));
	if(thiz == NULL) return NULL;

	thiz->parent = parent;
	thiz->process = module_uwsgi_handle_request;
	pool_add_cleanup(pool, module_uwsgi_destroy, thiz);
	array_init(&thiz->pass_params, pool, 10);

	module_uwsgi_priv_t* priv = (module_uwsgi_priv_t* ) thiz->priv;

	//sync the params into module_uwsgi_priv_t.	
	module_param_t** p = (module_param_t** ) params->elts;
	int i = 0;
	for(; i<params->count; i++)
	{
		str_t* name = &(p[i]->name);
		str_t** values = (str_t** )p[i]->values.elts;
		if(strncmp(name->data, "ip") == 0)
		{
			assert(p[i]->values.count == 1);
			priv->ip.data = pool_strdup(pool, values[0]->data);
			priv->ip.len = name->len;
		}
		else if(strncmp(name->data, "param") == 0)
		{
			int k = 0;	
			for(; k<)
		}
	}

	return thiz;
}

static int module_uwsgi_handle_request(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);
	
	//create upstream_uwsgi.
	upstream_t* upstream = upstream_uwsgi_create(request->pool);

	if(upstream == NULL)
	{
		request->response.status = HTTP_STATUS_INTERNAL_SERVER_ERROR;

		return HTTP_MODULE_PROCESS_DONE;
	}

	request->upstream = upstream;

	return HTTP_MODULE_PROCESS_UPSTREAM;
}

void module_get_info(module_so_conf_t* so_conf, pool_t* pool)
{
	assert(so_conf!=NULL && pool!=NULL);

	to_string(so_conf->name, "module_uwsgi");
	to_string(so_conf->author, "pengwu<wp.4163196@gmail.com>");
	to_string(so_conf->description, "uwsgi request handler");
	to_string(so_conf->version, "0.1");
	so_conf->module_create = module_uwsgi_create;

	return;
}

