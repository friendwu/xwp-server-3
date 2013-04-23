#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "typedef.h"
#include "module.h"

typedef struct module_default_priv_s 
{
	char priv[1];
}module_default_priv_t;

static int module_default_handle_request(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request!=NULL);
	http_response_t* response = &request->response;

	if(request->method != HTTP_METHOD_GET)
	{
		response->status = HTTP_STATUS_BAD_REQUEST;
		return HTTP_MODULE_PROCESS_DONE;
	}
	
	conf_t* root_conf = ((vhost_loc_conf_t*)thiz->parent)->parent->parent;
	//TODO check there is no '..' contains in the path.
	pool_t* pool = request->pool;
	vhost_loc_conf_t* loc = (vhost_loc_conf_t* )thiz->parent;
	str_t request_path = {0};

	if(request->url.path.len==1 && request->url.path.data[0]=='/')
	{
		request_path.data = root_conf->default_page.data;
		request_path.len = root_conf->default_page.len;
	}
	else
	{
		request_path = request->url.path;
	}
	
	char* path = (char* )pool_calloc(pool, loc->root.len + request_path.len + 2);

	sprintf(path, "%s/%s", loc->root.data, request_path.data);
	
	struct stat st = {0};
	if(stat(path, &st) != 0)
	{
		response->status = HTTP_STATUS_NOT_FOUND;
		printf("stat %s failed: %s\n", path, strerror(errno));

		return HTTP_MODULE_PROCESS_DONE;
	}

	//TODO allow list dir.
	if(S_ISDIR(st.st_mode) || 
		!(S_ISREG(st.st_mode) && (S_IRUSR & st.st_mode)))
	{
		response->status = HTTP_STATUS_FORBIDDEN;

		return HTTP_MODULE_PROCESS_DONE;
	}

	int fd = open(path, O_RDONLY);
	if(fd < 0) 
	{
		response->status = HTTP_STATUS_NOT_FOUND;

		return HTTP_MODULE_PROCESS_DONE;
	}
	
	response->content_fd = fd;
	response->content_len = st.st_size;

	char* extension = strrchr(request->url.path.data, '.');
	if(extension != NULL) extension += 1;
	
	http_header_set(&response->headers, HTTP_HEADER_CONTENT_TYPE, http_content_type(extension));
	response->status = HTTP_STATUS_OK;
	
	return HTTP_MODULE_PROCESS_DONE;
}

static void module_default_destroy(void* data)
{
	//TODO cleanup 
	return;
}

module_t* module_default_create(void* parent, array_t* params, pool_t* pool)
{
	module_t* thiz = pool_calloc(pool, sizeof(module_t) + sizeof(module_default_priv_t));
	if(thiz == NULL) return NULL;

	thiz->parent = parent;
	thiz->process = module_default_handle_request;
	pool_add_cleanup(pool, module_default_destroy, thiz);

	return thiz;
}



void module_get_info(module_so_conf_t* so_conf, pool_t* pool)
{
	assert(so_conf!=NULL && pool!=NULL);

	to_string(so_conf->name, "module_default");
	to_string(so_conf->author, "pengwu<wp.4163196@gmail.com>");
	to_string(so_conf->description, "default request handler");
	to_string(so_conf->version, "0.1");
	so_conf->module_create = module_default_create;

	return;
}

