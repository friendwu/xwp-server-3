#include <string.h>
#include <stdint.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include "typedef.h"
#include "utils.h"
#include "http.h"
#include "module.h"
#include "upstream.h"

const str_t CGI_SCRIPT_FILENAME = server_string("SCRIPT_FILENAME");
const str_t CGI_QUERY_STRING = server_string("QUERY_STRING");
const str_t CGI_REQUEST_METHOD = server_string("REQUEST_METHOD");
const str_t CGI_CONTENT_TYPE = server_string("CONTENT_TYPE");
const str_t CGI_CONTENT_LENGTH = server_string("CONTENT_LENGTH");
const str_t CGI_SCRIPT_NAME = server_string("SCRIPT_NAME");
const str_t CGI_REQUEST_URI = server_string("REQUEST_URI");
const str_t CGI_DOCUMENT_URI = server_string("DOCUMENT_URI");
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

typedef struct module_uwsgi_priv_s 
{
	str_t ip;
	uint16_t port;

	uint8_t modifier1;
	uint8_t modifier2;

	array_t pass_params;
}module_uwsgi_priv_t;

typedef struct upstream_uwsgi_priv_s
{
	int fd;
	int running;
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

	DECL_PRIV(module, module_priv, module_uwsgi_priv_t*);
	DECL_PRIV(thiz, upstream_priv, upstream_uwsgi_priv_t*);

	//TODO
	upstream_priv->fd = connect_remote(module_priv->ip.data, module_priv->port);

	return thiz;
}

static int upstream_uwsgi_add_param(char* buf, str_t* name, str_t* value)
{
	int count = 0;
	int pos = 0;
	
	uint16_little_endian(buf+pos, (uint16_t)name->len);
	pos += 2;
	count = sprintf(buf+pos, "%s", name->data);
	pos += count;

	uint16_little_endian(buf+pos, (uint16_t)name->len);
	pos += 2;
	count = sprintf(buf+pos, "%s", name->data);
	pos += count;

	return pos;
}

static uint16_t upstream_uwsgi_calc_packet_len(upstream_t* thiz, http_request_t* request)
{
	uint16_t datasize = 0;
	datasize += CGI_QUERY_STRING.len + request->url.query_string.len + 4;
	datasize += CGI_REQUEST_METHOD.len + request->method_str.len + 4;
	datasize += CGI_CONTENT_TYPE.len + request->content_type_header->len + 4;
	datasize += CGI_CONTENT_LENGTH.len + request->content_len_header->len + 4;
	datasize += CGI_REQUEST_URI.len + request->url.unparsed_url.len + 4;
	datasize += CGI_DOCUMENT_URI.len + request->url.path.len + 4; 
	datasize += CGI_DOCUMENT_ROOT.len + loc_conf->root.len + 4;
	datasize += CGI_SERVER_PROTOCOL.len + request->version_str.len + 4;
	datasize += CGI_GATEWAY_INTERFACE.len + sizeof("CGI/1.1") - 1 + 4;
	datasize += CGI_SERVER_SOFTWARE.len + sizeof(XWP_SERVER_VER) - 1 + 4;
	datasize += CGI_REMOTE_ADDR.len + request->remote_ip.len + 4;
	datasize += CGI_SERVER_ADDR.len + root_conf->ip.len + 4;
	char tmp[64];
	int count = sprintf(tmp, "%d", root_conf->port);
	datasize += CGI_SERVER_PORT.len + count + 4;
	datasize += CGI_SERVER_NAME.len + loc_conf->parent->name.len + 4;

	str_t** headers = request->headers.elts;
	int header_count = request->headers.count;

	int i = 0;
	for(; i<header_count; i++)
	{
		str_t* name = &headers[i].name;
		str_t* value = &headers[i].value;

		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_TYPE->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;
		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_LEN->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;

		datasize += sizeof("HTTP_") - 1 + name->len + value->len;
	}

	return datasize;
}

static int upstream_uwsgi_alloc_large_header_buf(upstream_t* thiz, buf_t* buf)
{
	conf_t* root_conf = ((vhost_loc_conf_t*) thiz->module->parent)->parent->parent;

	int rest_len = buf->last - buf->pos;
	int old_pos = buf->pos;

	if(root_conf->large_header_size <= rest_len)
	{
		return 0;
	}
	buf_create(buf, thiz->r.pool, thiz->conf->large_client_header_size + 1);

	if(buf->start == NULL) return 0;

	*(buf->end - 1) = '\0'; 
	buf->end -= 1;
	if(rest_len > 0)
	{
		memcpy(buf->start, old_pos, rest_len);
		buf->last += rest_len;
	}

	return 1;
}

static void upstream_uwsgi_process(upstream_t* thiz, http_request_t* request)
{
	conf_t* root_conf = ((vhost_loc_conf_t*) thiz->module->parent)->parent->parent;
	vhost_loc_conf_t* loc_conf = (vhost_loc_conf_t*) (thiz->module->parent);

	DECL_PRIV(thiz->module, module_priv, module_uwsgi_priv_t*);
	DECL_PRIV(thiz, upstream_priv, upstream_uwsgi_priv_t*);
	upstream_priv->running = 1;

	uint16_t datasize = upstream_uwsgi_calc_packet_len(thiz, request);
	char* buf = (char* )pool_calloc(pool, datasize + 4); //4 means the length of packet header.
	
	if(buf == NULL) request->response.status = HTTP_STATUS_BAD_REQUEST;

	int pos = 0;
	buf[pos++] = module_priv->modifier1;
	uint16_little_endian(buf+pos, datasize);
	pos += 2;
	buf[pos++] = module_priv->modifier2;

	pos += upstream_uwsgi_add_param(buf+pos, &CGI_QUERY_STRING, &request->url.query_string);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_REQUEST_METHOD, &request->method_str);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_CONTENT_TYPE, request->content_type_header);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_CONTENT_LENGTH, request->content_len_header);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_REQUEST_URI, &request->url.unparsed_url);
	pos += upstream_uwsgi_add_param(buf+pos, &CGI_DOCUMENT_URI, &request->url.path);
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
	//TODO params from config.

	str_t** headers = request->headers.elts;
	int header_count = request->headers.count;

	int i = 0;
	for(; i<header_count; i++)
	{
		str_t* name = &headers[i].name;
		str_t* value = &headers[i].value;

		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_TYPE->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;
		if(strncasecmp(name->data, HTTP_HEADER_CONTENT_LEN->data, HTTP_HEADER_CONTENT_TYPE->len) == 0)
			continue;

		uint16_little_endian(buf+pos, (uint16_t)name->len);
		pos += 2;
		int count = sprintf(buf+pos, "HTTP_%s", name->data);
		char* p = buf + pos + sizeof("HTTP_") - 1;
		for(; *p!='\0'; p++) 
		{
			if(*p == '-') *p = '_';
			else if(islower(*p)) *p = toupper(*p);
		}

		pos += count;
		uint16_little_endian(buf+pos, (uint16_t)value->len);
		pos += 2;
		count = sprintf(buf+pos, "%s", value->data);
		pos += count;
	}

	assert(pos == datasize);

	if(!nwrite(upstream_priv->fd, buf, datasize))
	{
		request->response->status = HTTP_STATUS_BAD_GATEWAY;

		goto DONE;
	}

	if(request->content_len > 0)
	{
		if(!nwrite(upstream_priv->fd, request->body_buf.pos, request->content_len))
		{
			request->response->status = HTTP_STATUS_BAD_GATEWAY;

			goto DONE;
		}
	}

	enum 
	{
		STAT_PARSE_STATUS_LINE = 0,
		STAT_PARSE_HEADER_LINE,
		STAT_PARSE_BODY,
		STAT_PARSE_DONE,
	}state = STAT_PARSE_STATUS_LINE;

	int parse_ret = HTTP_PARSE_AGAIN;
	buf_t* buf = &upstream_priv->header_buf;
	for(;;)
	{
		if(buf->last == buf->end)
		{
			assert(state <= STAT_PARSE_HEADER_LINE);
			if(!upstream_uwsgi_alloc_large_header_buf(thiz, &thiz->r.header_buf))
			{
				request->response.status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
				goto DONE;
			}

		}

		recv(upstream_priv->fd, buf->last, buf->end-buf->last, 0);
		if(count <= 0)
		{
			request->response.status = HTTP_STATUS_BAD_GATEWAY;
			break;
		}
		buf->last += count;
PARSE:
		switch(state)
		{
			case STAT_PARSE_STATUS_LINE:
				parse_ret = http_parse_status_line(request, buf, &request->response.status);
				if(parse_ret == HTTP_PARSE_DONE)
				{
					state = STAT_PARSE_HEADER_LINE;
				}
				break;

			case STAT_PARSE_HEADER_LINE:
				parse_ret = http_parse_header_line(request, buf, request->response.headers);
				if(parse_ret == HTTP_PARSE_DONE)
				{
					request->response.content_len = http_header_int(request->response.headers, HTTP_HEADER_CONTENT_LEN);
					if(request->response.content_len <= 0)
					{
						state = STAT_PARSE_DONE;
						request->response.content_len = 0;
					}
					else
					{
						state = STAT_PARSE_BODY;
					}
				}
				break;

			case STAT_PARSE_BODY:
				parse_ret = http_parse_content_body(request, buf, 0, content_len);
				break;
			case STAT_PARSE_DONE:
			default:
				assert(0);
				break;
		}

		if(state!=STAT_PARSE_DONE && 
				parse_ret==HTTP_PARSE_DONE)
		{
			goto PARSE;
		}
		else if(parse_ret==HTTP_PARSE_FAIL)
		{
			thiz->r.response.status = HTTP_STATUS_BAD_REQUEST;
			goto DONE;
		}
		if(state == STAT_PARSE_DONE)
		{
			thiz->r.response.status = HTTP_STATUS_OK;
			goto DONE;
		}
	}
DONE:
	thiz->running = 0;
	return;
}

static int upstream_uwsgi_abort(upstream_t* thiz)
{
	DECL_PRIV(thiz, priv, upstream_uwsgi_priv_t*);

	if(priv->fd > 0 && priv->running)
	{
		shutdown(priv->fd);
		while(priv->running) usleep(3000);
	}

	return 1;
}

static void upstream_uwsgi_destroy(void* data)
{
	//TODO cleanup
	upstream_t* thiz = (upstream_t* data);
	assert(thiz != NULL);

	DECL_PRIV(thiz, priv, upstream_uwsgi_priv_t*);

	close(priv->fd);

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
		/*
		else if(strncmp(name->data, "param") == 0)
		{
			int k = 0;
			for(; k<)
		}*/
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

