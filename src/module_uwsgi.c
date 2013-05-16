#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>
#include "upstream.h"
#include "typedef.h"
#include "utils.h"
#include "log.h"
//#include "http.h"
#include <sys/types.h>        
#include <sys/socket.h>
#include "module.h"
#include "xml_tree.h"

const str_t CGI_SCRIPT_FILENAME = server_string("SCRIPT_FILENAME");
const str_t CGI_QUERY_STRING = server_string("QUERY_STRING");
const str_t CGI_REQUEST_METHOD = server_string("REQUEST_METHOD");
const str_t CGI_CONTENT_TYPE = server_string("CONTENT_TYPE");
const str_t CGI_CONTENT_LENGTH = server_string("CONTENT_LENGTH");
const str_t CGI_SCRIPT_NAME = server_string("SCRIPT_NAME");
const str_t CGI_REQUEST_URI = server_string("REQUEST_URI");
const str_t CGI_PATH_INFO = server_string("PATH_INFO");
const str_t CGI_DOCUMENT_ROOT = server_string("DOCUMENT_ROOT");
const str_t CGI_SERVER_PROTOCOL = server_string("SERVER_PROTOCOL");
const str_t CGI_GATEWAY_INTERFACE = server_string("GATEWAY_INTERFACE");
const str_t CGI_SERVER_SOFTWARE = server_string("SERVER_SOFTWARE");
const str_t CGI_REMOTE_ADDR = server_string("REMOTE_ADDR");
const str_t CGI_SERVER_ADDR = server_string("SERVER_ADDR");
const str_t CGI_SERVER_PORT = server_string("SERVER_PORT");
const str_t CGI_SERVER_NAME = server_string("SERVER_NAME");
const str_t CGI_REDIRECT_STATUS = server_string("REDIRECT_STATUS");

/*const str_t CGI_HTTP_ACCEPT = server_string("HTTP_ACCEPT");
const str_t CGI_HTTP_ACCEPT_LANGUAGE = server_string("HTTP_ACCEPT_LANGUAGE");
const str_t CGI_HTTP_ACCEPT_ENCODING = server_string("HTTP_ACCEPT_ENCODING");
const str_t CGI_HTTP_USER_AGENT = server_string("HTTP_USER_AGENT");
const str_t CGI_HTTP_HOST = server_string("HTTP_HOST");
const str_t CGI_HTTP_CONNECTION = server_string("HTTP_CONNECTION");
const str_t CGI_HTTP_CONTENT_TYPE = server_string("HTTP_CONTENT_TYPE");
const str_t CGI_HTTP_CONTENT_LENGTH = server_string("HTTP_CONTENT_LENGTH");
const str_t CGI_HTTP_CACHE_CONTROL = server_string("HTTP_CACHE_CONTROL");
const str_t CGI_HTTP_COOKIE = server_string("HTTP_COOKIE");
const str_t CGI_HTTP_REFERER = server_string("HTTP_REFERER");*/

typedef struct module_uwsgi_conf_s
{
	str_t ip;
	uint16_t port;

	uint8_t modifier1;
	uint8_t modifier2;

	array_t* pass_params; //module_param_t*
}module_uwsgi_conf_t;

typedef struct module_uwsgi_priv_s 
{
	const vhost_loc_conf_t* loc_conf;
	module_uwsgi_conf_t conf;
}module_uwsgi_priv_t;

typedef struct upstream_uwsgi_priv_s
{
	int fd;
	int running;
	pthread_mutex_t mutex;
}upstream_uwsgi_priv_t;

static int upstream_uwsgi_add_param(char* buf, const str_t* name, const str_t* value)
{
	assert(name != NULL && name->data != NULL && name->len > 0);
	assert(name->data[name->len] == '\0' 
			&& name->data[name->len - 1] != '\0');
	int pos = 0;
	
	uint16_little_endian(buf+pos, (uint16_t)name->len);
	pos += 2;
	sprintf(buf+pos, "%s", name->data);
	pos += name->len;

	if(value!=NULL && value->len>0)
	{
		//strlen(value->data) == value->len
		assert(value->data[value->len] == '\0' 
				&& value->data[value->len - 1] != '\0');

		uint16_little_endian(buf+pos, (uint16_t)value->len);
		pos += 2;
		sprintf(buf+pos, "%s", value->data);
		pos += value->len;
	}
	else
	{
		uint16_little_endian(buf+pos, 0);
		pos += 2;
	}

	return pos;
}

static uint16_t upstream_uwsgi_calc_packet_len(upstream_t* thiz, http_request_t* request)
{
	DECL_PRIV(thiz->module, module_priv, module_uwsgi_priv_t*);
	const conf_t* root_conf = module_priv->loc_conf->parent->parent;
	const vhost_loc_conf_t* loc_conf = module_priv->loc_conf;

	uint16_t datasize = 0;
	//plus 4 means key_size(uint16_t) and val_size(uint16_t)
	datasize += CGI_QUERY_STRING.len + request->url.query_string.len + 4;
	datasize += CGI_REQUEST_METHOD.len + request->method_str.len + 4;

	if(request->headers_in.header_content_type != NULL
			&& request->headers_in.header_content_type->len > 0)
	{
		datasize += CGI_CONTENT_TYPE.len + request->headers_in.header_content_type->len + 4;
	}
	else
	{
		datasize += CGI_CONTENT_TYPE.len + 4;
	}

	if(request->headers_in.header_content_len != NULL
			&& request->headers_in.header_content_len->len > 0)
	{
		datasize += CGI_CONTENT_LENGTH.len + request->headers_in.header_content_len->len + 4;
	}
	else
	{
		datasize += CGI_CONTENT_LENGTH.len + 4;
	}

	datasize += CGI_REQUEST_URI.len + request->url.unparsed_url.len + 4;
	datasize += CGI_PATH_INFO.len + request->url.path.len + 4; 
	datasize += CGI_DOCUMENT_ROOT.len + loc_conf->root.len + 4;
	datasize += CGI_SERVER_PROTOCOL.len + request->version_str.len + 4;
	//datasize += CGI_GATEWAY_INTERFACE.len + sizeof("CGI/1.1") - 1 + 4;
	datasize += CGI_SERVER_SOFTWARE.len + sizeof(XWP_SERVER_VER) - 1 + 4;
	datasize += CGI_REMOTE_ADDR.len + request->remote_ip.len + 4;
	datasize += CGI_SERVER_ADDR.len + root_conf->ip.len + 4;
	char tmp[64];
	int count = sprintf(tmp, "%d", root_conf->port);
	datasize += CGI_SERVER_PORT.len + count + 4;
	datasize += CGI_SERVER_NAME.len + loc_conf->parent->name.len + 4;

	http_header_t** header_elts = (http_header_t** )request->headers_in.headers->elts;
	int header_count = request->headers_in.headers->count;

	int i = 0;
	for(; i<header_count; i++)
	{
		str_t* name = &header_elts[i]->name;
		str_t* value = &header_elts[i]->value;

		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_TYPE->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;
		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_LEN->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;

		datasize += sizeof("HTTP_") - 1 + name->len + value->len + 4;
	}

	return datasize;
}

static void upstream_uwsgi_process(upstream_t* thiz)
{
	http_request_t* request = thiz->request;
	DECL_PRIV(thiz->module, module_priv, module_uwsgi_priv_t*);
	DECL_PRIV(thiz, upstream_priv, upstream_uwsgi_priv_t*);

	const conf_t* root_conf = module_priv->loc_conf->parent->parent;
	const vhost_loc_conf_t* loc_conf = module_priv->loc_conf; 
	upstream_priv->running = 1;

	uint16_t datasize = upstream_uwsgi_calc_packet_len(thiz, request);
	/*
		4 means the length of packet header.
		1 means the trailing \0. as every time the sprintf func will 
		write a \0 after the string content.
	*/
	char* buf = pool_calloc(request->pool, datasize + 4 + 1); 
	
	if(buf == NULL) request->status = HTTP_STATUS_BAD_REQUEST;

	int pos = 0;
	buf[pos++] = module_priv->conf.modifier1;
	uint16_little_endian(buf+pos, datasize);
	pos += 2;
	buf[pos++] = module_priv->conf.modifier2;

	pos += upstream_uwsgi_add_param(buf+pos, &CGI_QUERY_STRING, &request->url.query_string);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_REQUEST_METHOD, &request->method_str);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_CONTENT_TYPE, request->headers_in.header_content_type);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_CONTENT_LENGTH, request->headers_in.header_content_len);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_REQUEST_URI, &request->url.path);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_PATH_INFO, &request->url.unparsed_url);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_DOCUMENT_ROOT, &loc_conf->root);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_SERVER_PROTOCOL, &request->version_str);
	//pos += upstream_uwsgi_add_param(buf+pos, &CGI_GATEWAY_INTERFACE, "CGI/1.1") - 1);
	
	str_t xwp_server_ver = server_string(XWP_SERVER_VER);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_SERVER_SOFTWARE, &xwp_server_ver);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_REMOTE_ADDR, &request->remote_ip);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_SERVER_ADDR, &root_conf->ip);

	char str[64];
	int count = sprintf(str, "%d", root_conf->port);
	str_t port = {str, count};
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_SERVER_PORT, &port);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_SERVER_NAME, &loc_conf->parent->name);
	//TODO params from module_uwsgi_conf_t.

	http_header_t** header_elts = (http_header_t** )request->headers_in.headers->elts;
	int header_count = request->headers_in.headers->count;

	int i = 0;
	for(; i<header_count; i++)
	{
		str_t* name = &header_elts[i]->name;
		str_t* value = &header_elts[i]->value;

		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_TYPE->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;
		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_LEN->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;

		uint16_little_endian(buf+pos, (uint16_t)(name->len + sizeof("HTTP_") - 1));
		pos += 2;
		int count = sprintf(buf+pos, "HTTP_%s", name->data);

		char* p = buf + pos + sizeof("HTTP_") - 1;
		for(; *p!='\0'; p++) 
		{
			if(*p == '-') *p = '_';
			else if(islower(*p)) *p = toupper(*p);
		}
		pos += count;

		if(value!=NULL && value->len>0)
		{
			uint16_little_endian(buf+pos, (uint16_t)value->len);
			pos += 2;

			count = sprintf(buf+pos, "%s", value->data);
			pos += count;
		}
		else
		{
			uint16_little_endian(buf+pos, 0);
			pos += 2;
		}
	}

	log_debug("pos %d datasize %d iplen %d\n", pos, datasize + 4, request->remote_ip.len);
	assert(pos == datasize + 4); //4 means packet header length.

	if(!nwrite(upstream_priv->fd, buf, datasize + 4))
	{
		request->status = HTTP_STATUS_BAD_GATEWAY;

		goto DONE;
	}

	if(request->body_in.content_len > 0)
	{
		if(!nwrite(upstream_priv->fd, request->body_in.content->pos, request->body_in.content_len))
		{
			request->status = HTTP_STATUS_BAD_GATEWAY;

			goto DONE;
		}
	}

	//start read and parsing response from backend.
	int ret = 0;
	ret = http_process_status_line(request, upstream_priv->fd);
	if(ret == 0) goto DONE;
	
	ret = http_process_header_line(request, upstream_priv->fd, HTTP_PROCESS_PHASE_RESPONSE);
	if(ret == 0) goto DONE;

	http_process_content_body(request, upstream_priv->fd, 
							  &request->body_out, request->headers_out.content_len);

DONE:
	//FIXME tmp.
	if(ret == 0) request->status = HTTP_STATUS_BAD_GATEWAY;
	upstream_priv->running = 0;

	pthread_mutex_lock(&upstream_priv->mutex);
	if(upstream_priv->fd >= 0)
	{
		close(upstream_priv->fd);
		upstream_priv->fd = -1;
	}
	pthread_mutex_unlock(&upstream_priv->mutex);

	return;
}

static int upstream_uwsgi_abort(upstream_t* thiz)
{
	DECL_PRIV(thiz, upstream_priv, upstream_uwsgi_priv_t*);

	pthread_mutex_lock(&upstream_priv->mutex);
	if(upstream_priv->fd >= 0 && upstream_priv->running)
	{
		shutdown(upstream_priv->fd, SHUT_RDWR);
		close(upstream_priv->fd);
		upstream_priv->fd = -1;
		while(upstream_priv->running) usleep(1000);
	}
	pthread_mutex_unlock(&upstream_priv->mutex);

	return 1;
}

static void upstream_uwsgi_destroy(void* data)
{
	assert(data != NULL);
	upstream_t* thiz = (upstream_t* )data;

	upstream_uwsgi_abort(thiz);

	DECL_PRIV(thiz, upstream_priv, upstream_uwsgi_priv_t*);
	pthread_mutex_destroy(&upstream_priv->mutex);

	return;
}

static upstream_t* upstream_uwsgi_create(http_request_t* request, module_t* module)
{
	upstream_t* thiz = pool_alloc(request->pool, sizeof(upstream_t) + sizeof(upstream_uwsgi_priv_t));

	if(thiz == NULL) 
	{
		request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		return NULL;
	}
	

	pool_add_cleanup(request->pool, upstream_uwsgi_destroy, thiz);
	
	thiz->request = request;
	thiz->process = upstream_uwsgi_process;
	thiz->abort = upstream_uwsgi_abort;
	thiz->module = module;

	DECL_PRIV(module, module_priv, module_uwsgi_priv_t*);
	DECL_PRIV(thiz, upstream_priv, upstream_uwsgi_priv_t*);

	pthread_mutex_init(&upstream_priv->mutex, NULL);

	upstream_priv->fd = connect_remote(module_priv->conf.ip.data, module_priv->conf.port);

	if(upstream_priv->fd < 0)
	{
		request->status = HTTP_STATUS_BAD_GATEWAY;

		return NULL;
	}

	return thiz;
}

static void module_uwsgi_destroy(void* data)
{
	//TODO cleanup 
	return;
}

static int module_uwsgi_conf_init(module_uwsgi_conf_t* conf, XmlNode* xml_conf_node, pool_t* pool)
{
	/*conf->modifier1 = 0;
	conf->modifier2 = 0;
	to_string(conf->ip, "127.0.0.1");
	conf->port = 9000;*/
	conf->modifier1 = xml_tree_int_first(xml_conf_node, "modifier1", 0);
	conf->modifier1 = xml_tree_int_first(xml_conf_node, "modifier2", 0);

	XmlNode* pass_conf_node = xml_tree_find_first(xml_conf_node, "uwsgi_pass");
	if(pass_conf_node == NULL)
	{
		log_error("uwsgi_pass config needed.");

		return 0;
	}

	pool_strdup2(pool, &conf->ip, xml_tree_str_first(pass_conf_node, "ip"));
	if(conf->ip.data == NULL) to_string(conf->ip, "127.0.0.1");
	conf->port = xml_tree_int_first(pass_conf_node, "port", -1);
	if(conf->port == -1) 
	{
		log_error("uwsgi_pass port needed.");

		return 0;
	}

	return 1;
}

static int module_uwsgi_handle_request(module_t* thiz, http_request_t* request)
{
	assert(thiz!=NULL && request != NULL);
	
	upstream_t* upstream = upstream_uwsgi_create(request, thiz);

	if(upstream == NULL)
	{
		//status has been set in upstream_uwsgi_create 
		assert(request->status > HTTP_STATUS_SPECIAL_RESPONSE);

		return HTTP_MODULE_PROCESS_DONE;
	}

	request->upstream = upstream;

	return HTTP_MODULE_PROCESS_UPSTREAM;
}

static module_t* module_uwsgi_create(void* ctx, XmlNode* xml_conf_node, pool_t* pool)
{
	assert(ctx!=NULL && pool!=NULL);

	module_t* thiz = pool_calloc(pool, sizeof(module_t) + sizeof(module_uwsgi_priv_t));
	if(thiz == NULL) return NULL;

	DECL_PRIV(thiz, module_priv, module_uwsgi_priv_t*);
		
	if(!module_uwsgi_conf_init(&module_priv->conf, xml_conf_node, pool))
	{
		return NULL;
	}

	module_priv->loc_conf = (vhost_loc_conf_t*)ctx;
	thiz->process = module_uwsgi_handle_request;

	pool_add_cleanup(pool, module_uwsgi_destroy, thiz);

	return thiz;
}

void module_get_info(module_so_conf_t* so_conf, pool_t* pool)
{
	assert(so_conf!=NULL && pool!=NULL);

	to_string(so_conf->name, "uwsgi");
	to_string(so_conf->author, "pengwu<wp.4163196@gmail.com>");
	to_string(so_conf->description, "uwsgi request handler");
	to_string(so_conf->version, "0.1");
	so_conf->module_create = module_uwsgi_create;

	return;
}

