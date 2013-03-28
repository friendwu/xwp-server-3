#include <string.h>
#include <sys/stat.h>
#include <utils.h>
#include "module.h"

typedef struct _ModuleDefaultPriv
{
	char priv[1];
}ModuleDefaultPriv;

static Ret module_default_handle_req(Module* thiz, const HttpReqCtx* req_ctx, 
									 const HttpReq* req, HttpResp* resp)
{
	if(strcasecmp(req->method, "GET") != 0)
	{
		resp->status = HTTP_STATUS_BAD_REQUEST;

		//TODO should we add status file?
		return RET_OK;
	}
	
	//TODO check there is no '..' contains in the path.
	char path_buf[MAX_PATH_LEN] = {0};
	snprintf(path_buf, "%s/%s", req_ctx->root, req->path);

	struct stat st = {0};
	if(stat(path_buf, &st) != 0)
	{
		resp->status = HTTP_STATUS_NOT_FOUND;
		printf("stat %s failed: %s\n", path_buf, strerror(errno));

		return RET_OK;
	}

	//TODO allow list dir.
	if(!(S_ISDIR(st.st_mode) && req_ctx->allow_list_dir) || 
		!(S_ISREG(st.st_mode) && (S_IRUSR & st.st_mode)))
	{
		resp->status = HTTP_STATUS_FORBIDDEN;

		return RET_OK;
	}

	int fd = open(path_buf, O_RDONLY);
	if(fd < 0) 
	{
		resp->status = HTTP_STATUS_NOT_FOUND;

		return RET_OK;
	}
	
	resp->content_fd = fd;
	resp->content_len = st.st_size;

	char* extension = strrchr(req->path, '.');
	if(extension != NULL) extension += 1;

	http_header_set(resp->headers, HTTP_HEADER_CONENT_TYPE, get_file_content_type(extension));
	resp->status = HTTP_STATUS_OK;
	
	return RET_OK;
}

static Ret module_default_destroy(Module* thiz)
{
	free(thiz);

	return RET_OK;
}

Module* module_default_create(ModuleFactory* factory, ModuleParam param[])
{
	Module* thiz = calloc(1, sizeof(Module) + sizeof(ModuleDefaultPriv));
	if(thiz == NULL) return NULL;

	thiz->parent = factory;
	thiz->handle = module_default_handle_req;
	thiz->destroy_module = module_default_destroy;

	return thiz;
}

static Ret module_factory_default_destroy(ModuleFactory* thiz)
{
	free(thiz);

	return RET_OK;
}

ModuleFactory* module_factory_create(Server* server)
{
	ModuleFactory* thiz = calloc(1, sizeof(ModuleFactory));
	if(thiz == NULL) return NULL;

	thiz->name = "module_default";
	thiz->author = "pengwu<wp.4163196@gmail.com>";
	thiz->description = "default request handler";
	thiz->version = "0.1";
	
	thiz->server = server;

	thiz->create_module = module_default_create;
	thiz->destroy_module_factory = module_factory_default_destroy;

	return thiz;
}
