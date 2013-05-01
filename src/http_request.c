#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <ctype.h>
#include <string.h>
#include "conf.h"
#include "http.h"
#include "utils.h"

#define eol(c) ((c) == '\n')
#define is_character(c) (((c)|0x20) >='a' && ((c)|0x20) <= 'z')
#define HTTP_PARSE_DONE 1
#define HTTP_PARSE_FAIL 0
#define HTTP_PARSE_AGAIN -1

static char* find_next_eol(char* line)
{
	char* p = line;
	
	while(!eol(*p) && *p!='\0') p++;

	if(*p == '\0')
		return NULL;
	else 
		return p;
}

static int http_parse_url(http_request_t* request, url_t* url, str_t* url_str)
{
	char* schema_start = NULL;
	char* schema_end = NULL;
	char* host_start = NULL;
	char* host_end = NULL;
	char* path_start = NULL;
	char* path_end = NULL;
	char* query_start = NULL;
	char* query_end = NULL;
	char* port_start = NULL;
	char* port_end = NULL;
	pool_t* pool = request->pool;

	url->unparsed_url.data = pool_strndup(pool, url_str->data, url_str->len);
	url->unparsed_url.len = url_str->len;

	enum 
	{
		STAT_START = 0,
		STAT_SCHEMA,
		STAT_SCHEMA_SLASH,
		STAT_HOST,
		STAT_PORT,
		STAT_PATH,
		STAT_QUERY_STRING,
		/*STAT_FRAGMENT_ID,*/
		STAT_DONE,
	}state = STAT_START;

	char* p = url_str->data;
	char* url_str_end = url_str->data + url_str->len;
	for(; p<url_str_end && *p!='\0'; p++)
	{
		switch(state)
		{
			case STAT_START:
			{
				if(isspace(*p)) break;
				
				if(is_character(*p))
				{
					schema_start = p;
					state = STAT_SCHEMA;
					break;
				}
				else if(*p == '/')
				{
					path_start = p;
					state = STAT_PATH;
					break;
				}
				else 
				{
					return HTTP_PARSE_FAIL;
				}
			}
			case STAT_SCHEMA:
			{
				if(is_character(*p)) break;

				if(*p == ':') 
				{
					schema_end = p;
					state = STAT_SCHEMA_SLASH;
					break;
				}
				else
				{
					return HTTP_PARSE_FAIL;
				}
			}
			case STAT_SCHEMA_SLASH:
			{
				if(*p == '/') break;
				else if(is_character(*p)) 
				{
					host_start = p;
					state = STAT_HOST;
					break;
				}
				else 
				{
					return HTTP_PARSE_FAIL;
				}
			}
			case STAT_HOST:
			{
				if(is_character(*p)) break;
				if(isdigit(*p) || *p == '.' || *p == '-')break;

				if(*p == ':') 
				{
					host_end = p;
					port_start = p + 1;
					state = STAT_PORT;
					break;
				}
				else if(*p == '/')
				{
					host_end = p;
					path_start = p;
					state = STAT_PATH;
					break;
				}
				else
				{
					return HTTP_PARSE_FAIL;
				}
			}
			case STAT_PORT:
			{
				if(isdigit(*p)) break;
				if(*p == '/') 
				{
					port_end = p;
					path_start = p;		
					state = STAT_PATH;
					break;
				}
				else if(isspace(*p))
				{
					port_end = p;
					state = STAT_DONE;
					break;
				}
				else
				{
					return HTTP_PARSE_FAIL;
				}
			}
			case STAT_PATH:
			{
				if(*p == '?') 
				{
					path_end = p;
					query_start = p + 1;
					state = STAT_QUERY_STRING;
					break;
				}
				else if(*p == '#')
				{
					//ignore the fragement id.
					path_end = p;
					state = STAT_DONE;
					break;
				}
				else if(isspace(*p))
				{
					path_end = p;
					state = STAT_DONE;
					break;
				}
				else 
				{
					break;
				}
			}
			case STAT_QUERY_STRING:
			{
				if(*p == '#' || isspace(*p))
				{
					query_end = p;
					state = STAT_DONE;
					break;
				}
				else
				{
					break;
				}
			}
			/*case STAT_FRAGMENT_ID:*/
			default:
			{
				assert(0);
			}
		}
		if(state == STAT_DONE) break;
	}

	if(schema_start)
	{
		if(!schema_end) return HTTP_PARSE_FAIL;
		*schema_end = '\0';

		url->schema.data = schema_start;//pool_strndup(pool, schema_start, schema_end-schema_start);
		url->schema.len = schema_end - schema_start;
	}
	if(host_start)
	{
		if(!host_end) return HTTP_PARSE_FAIL;

		url->host.data = pool_strndup(pool, host_start, host_end-host_start);
		url->host.len = host_end - host_start;
	}
	if(port_start)
	{
		if(!port_end) return HTTP_PARSE_FAIL;
		url->port = strtod(port_start, &port_end);
	}
	if(path_start)
	{
		if(!path_end) path_end = url_str_end;
		*path_end = '\0';
		
		url->path.data = path_start;
		url->path.len = path_end - path_start;
	}
	if(query_start)
	{
		if(!query_end) query_end = url_str_end;
		*query_end = '\0';

		url->query_string.data = query_start;//pool_strndup(pool, query_start, query_end-query_start);
		url->query_string.len = query_end - query_start;
	}

	return HTTP_PARSE_DONE;
}

static int http_parse_request_line(http_request_t* request, buf_t* buf)
{
	str_t method = {0};
	str_t url = {0};
	str_t version = {0};
	char* pos = buf->pos;
	char* last = buf->last;
	char* eol = NULL;

	//skip empty line first.
	while(isspace(*pos) && pos<last && *pos!='\0') pos++;
	buf->pos = pos;

	eol = find_next_eol(pos);
	if(eol == NULL)
	{
		return HTTP_PARSE_AGAIN;
	}
	
	int t1, t2, t3;
	t1 = get_token(&method, &pos, isspace, NULL);
	t2 = get_token(&url, &pos, isspace, NULL);
	t3 = get_token(&version, &pos, isspace, NULL);
	if(t1==0 || t2==0 || t3==0) return HTTP_PARSE_FAIL;
	buf->pos = eol + 1;

	if(method.data==NULL || url.data==NULL || version.data==NULL) return HTTP_PARSE_FAIL;

	request->version_str = version;
	version.data = strstr(version.data, "HTTP/");
	if(version.data == NULL) return HTTP_PARSE_FAIL;
	version.data += 5; //strlen("HTTP/")
	version.len = 3;

	if(strcmp(version.data, "1.0") == 0) request->version = HTTP_VERSION_10;
	else if(strcmp(version.data, "1.1") == 0) request->version = HTTP_VERSION_11;
	else return HTTP_PARSE_FAIL; //dont support 0.9

	if(strncasecmp(method.data, "GET", method.len) == 0) request->method = HTTP_METHOD_GET;
	else if(strncasecmp(method.data, "POST", method.len) == 0) request->method = HTTP_METHOD_POST;
	else if(strncasecmp(method.data, "HEAD", method.len) == 0) request->method = HTTP_METHOD_HEAD;
	else return HTTP_PARSE_FAIL; 

	request->method_str = method;

	return http_parse_url(request, &request->url, &url);
}

//FIXME  in one header may be multilines
static int http_parse_header_line(http_request_t* request, buf_t* buf, array_t* headers)
{
	assert(request!=NULL && buf!=NULL && headers!=NULL);

	int ret = HTTP_PARSE_AGAIN;
	char* eol = NULL;
	str_t name = {0};
	str_t value = {0};
	char* pos = buf->pos;
	char* last = buf->last;

	while(pos < last)	
	{
		eol = find_next_eol(pos);

		//partial line.
		if(eol == NULL) break; 

		//empty line.
		if(eol == pos || (eol==pos+1 && *pos=='\r' && *eol=='\n' ))
		{
			pos = eol + 1;
			ret = HTTP_PARSE_DONE;
			break;
		}

		//full line
		//FIXME should trim space.
		get_token(&name, &pos, NULL, ":");

		//skip empty line first.
		while(isspace(*pos) && pos<last && *pos!='\0') pos++;

		//get_token(&value, &pos, , NULL);

		if(pos == eol) 
		{
			value.data = NULL;
			value.len = NULL;
		}
		else
		{
			if(*(eol-1) == '\r')
			{
				*(eol-1) = '\0';
				value.data = pos;
				value.len = eol - 1 - pos;
			}
			else
			{
				*eol = '\0';
				value.data = pos;
				value.len = eol - pos;
			}
		}

		if(name.data == NULL) 
		{
			ret = HTTP_PARSE_FAIL;
			break;
		}

		if(!http_header_set(headers, &name, &value))
		{
			ret = HTTP_PARSE_FAIL;
			break;
		}

		pos = eol + 1;
	}

	buf->pos = pos;
	
	return ret;
}

static int http_parse_status_line(http_request_t* request, buf_t* buf, int* status)
{
	str_t ignore = {0};
	str_t status_str = {0};
	char* pos = buf->pos;
	char* last = buf->last;
	char* eol = NULL;

	//skip empty line first.
	while(isspace(*pos) && pos<last && *pos!='\0') pos++;
	buf->pos = pos;

	eol = find_next_eol(pos);
	if(eol == NULL)
	{
		return HTTP_PARSE_AGAIN;
	}
	
	//TODO check the return value.
	get_token(&ignore, &pos, isspace, NULL);
	get_token(&status_str, &pos, isspace, NULL);
	//ignore the short message.
	buf->pos = eol + 1;

	*status = atoi(status_str.data);
	if(*status < HTTP_STATUS_START || *status > HTTP_STATUS_END) 
		return HTTP_PARSE_FAIL;

	return HTTP_PARSE_DONE;
}

static int http_alloc_client_header_buf(http_request_t* request)
{
	request->header_buf = buf_create(request->pool, request->conf->client_header_size + 1);

	if(request->header_buf == NULL)
	{
		return 0;
	}

	*(request->header_buf->end - 1) = '\0'; 
	request->header_buf->end -= 1; 

	return 1;
}

static int http_alloc_large_client_header_buf(http_request_t* request)
{
	buf_t* header_buf = request->header_buf;

	if(header_buf->end-header_buf->start >= request->conf->large_client_header_size) 
	{
		return 0;
	}
	int rest_len = header_buf->last - header_buf->pos;
	char* old_pos = header_buf->pos;

	if(request->conf->large_client_header_size <= rest_len)
	{
		return 0;
	}
	request->header_buf = buf_create(request->pool, request->conf->large_client_header_size + 1);

	if(header_buf->start == NULL) return 0;

	*(header_buf->end - 1) = '\0'; 
	header_buf->end -= 1;
	if(rest_len > 0)
	{
		memcpy(header_buf->start, old_pos, rest_len);
		header_buf->last += rest_len;
	}

	return 1;
}

http_request_t* http_request_create(pool_t* pool, const conf_t* conf, struct sockaddr_in* peer_addr)
{
	assert(pool != NULL && conf != NULL);

	http_request_t* request = pool_calloc(pool, sizeof(http_request_t));
	request->pool = pool;
	request->conf = conf;

	request->state = HTTP_PROCESS_STAT_NONE;
	request->status = HTTP_STATUS_OK;

	//TODO maybe these memorys can be allocated in a more precise stage.
	request->headers_in.headers = array_create(pool, 24);
	if(request->headers_in.headers == NULL) return NULL;

	request->headers_out.headers = array_create(pool, 24);
	if(request->headers_out.headers == NULL) return NULL;

	request->body_in.content = NULL;
	request->body_in.content_fd = -1;

	request->body_out.content = NULL;
	request->body_out.content_fd = -1;

	request->peer_addr = peer_addr;
	request->remote_ip.data = inet_ntoa(peer_addr->sin_addr); 
	request->remote_ip.len = strlen(request->remote_ip.data); 
	request->remote_port = (uint16_t)peer_addr->sin_port; 

	return request;
}

int http_process_request_line(http_request_t* request, int fd)
{
	assert(request!=NULL && fd>=0 && request->state == HTTP_PROCESS_STAT_NONE);

	int parse = HTTP_PARSE_AGAIN;
	if(!http_alloc_client_header_buf(request))
	{
		request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		return 0;
	}

	request->state = HTTP_PROCESS_STAT_REQUEST_LINE;
	buf_t* buf = request->header_buf;
	for(;;)
	{
		parse = http_parse_request_line(request, buf);
		if(parse == HTTP_PARSE_AGAIN)
		{
			//meet buffer end but still parse incomplete.
			if(buf->last == buf->end) 
			{
				if(!http_alloc_large_client_header_buf(request))
				{
					//TODO maybe the status can be set in http_alloc_large_client_header_buf
					request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
					return 0;
				}
			}

			//TODO should we consider the situation of EINTR?
			int count = recv(fd, buf->last, buf->end-buf->last, 0);
			if(count <= 0) 
			{
				request->status = HTTP_STATUS_CLOSE;
				return 0;
			}
			
			buf->last += count;
		}
		else if(parse == HTTP_PARSE_FAIL)
		{
			request->status = HTTP_STATUS_BAD_REQUEST;
			return 0;
		}
		else
		{
			break;
		}
	}
	
	return 1;
}

int http_process_header_line(http_request_t* request, int fd, int process_phase)
{
	assert(request!=NULL && fd>=0);
	assert((process_phase==HTTP_PROCESS_PHASE_REQUEST 
				&& request->state==HTTP_PROCESS_STAT_REQUEST_LINE) 
			|| (process_phase==HTTP_PROCESS_PHASE_RESPONSE 
				&& request->state==HTTP_PROCESS_STAT_STATUS_LINE));

	int parse = HTTP_PARSE_AGAIN;
	buf_t* buf = request->header_buf;
	array_t* headers = NULL;

	if(process_phase == HTTP_PROCESS_PHASE_REQUEST)
		headers = request->headers_in.headers;
	else
		headers = request->headers_out.headers;

	request->state = HTTP_PROCESS_STAT_HEADER_LINE;
	for(;;)
	{
		parse = http_parse_header_line(request, buf, headers);
		if(parse == HTTP_PARSE_AGAIN)
		{
			//meet buffer end but still parse incomplete.
			if(buf->last == buf->end) 
			{
				if(!http_alloc_large_client_header_buf(request))
				{
					request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
					return 0;
				}
			}

			int count = recv(fd, buf->last, buf->end-buf->last, 0);
			if(count <= 0) 
			{
				request->status = HTTP_STATUS_CLOSE;
				return 0;
			}

			buf->last += count;
		}
		else if(parse == HTTP_PARSE_FAIL)
		{
			request->status = HTTP_STATUS_BAD_REQUEST;
			return 0;
		}
		else
		{
			break;
		}
	}

	if(process_phase == HTTP_PROCESS_PHASE_REQUEST)
	{
		http_headers_in_t* headers_in = &request->headers_in;
		headers_in->header_host = http_header_str(headers, HTTP_HEADER_HOST);
		if(headers_in->header_host == NULL)
		{
			request->status = HTTP_STATUS_BAD_REQUEST;
			return 0;
		}
		headers_in->header_content_type = http_header_str(headers, HTTP_HEADER_CONTENT_TYPE);
		headers_in->header_content_len = http_header_str(headers, HTTP_HEADER_CONTENT_LEN);

		headers_in->content_len = http_header_int(headers, HTTP_HEADER_CONTENT_LEN);
		if(headers_in->content_len < 0) headers_in->content_len = 0;

		const str_t* keep_alive = http_header_str(headers, HTTP_HEADER_KEEPALIVE);
		if(keep_alive == NULL)
		{
			if(request->version == HTTP_VERSION_11)
				request->keep_alive = 1;
			else 
				request->keep_alive = 0;
		}
		else
		{
			if(strncasecmp(keep_alive->data, "CLOSE", keep_alive->len) == 0)
				request->keep_alive = 0;
			else 
				request->keep_alive = 1;
		}
	}
	else 
	{
		http_headers_out_t* headers_out = &request->headers_out;

		headers_out->content_len = http_header_int(headers, HTTP_HEADER_CONTENT_LEN);
		if(headers_out->content_len < 0) headers_out->content_len = 0;
	}
	
	return 1;
}

int http_process_status_line(http_request_t* request, int fd)
{
	assert(request!=NULL && fd>=0 && request->state==HTTP_PROCESS_STAT_CONTENT_BODY);
	int parse = HTTP_PARSE_AGAIN;

	if(!http_alloc_client_header_buf(request))
	{
		request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		return 0;
	}

	request->state = HTTP_PROCESS_STAT_STATUS_LINE;
	buf_t* buf = request->header_buf;
	for(;;)
	{
		parse = http_parse_status_line(request, buf, &request->status);
		if(parse == HTTP_PARSE_AGAIN)
		{
			//meet buffer end but still parse incomplete.
			if(buf->last == buf->end) 
			{
				if(!http_alloc_large_client_header_buf(request))
				{
					request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
					return 0;
				}
			}

			int count = recv(fd, buf->last, buf->end-buf->last, 0);
			if(count <= 0) 
			{
				request->status = HTTP_STATUS_CLOSE;
				return 0;
			}
			buf->last += count;
		}
		else if(parse == HTTP_PARSE_FAIL)
		{
			request->status = HTTP_STATUS_BAD_REQUEST;
			return 0;
		}
		else
		{
			break;	
		}
	}
	
	return 1;
}

int http_process_content_body(http_request_t* request, int fd, http_content_body_t* content_body, int content_len)
{
	assert(request!=NULL && fd>=0 
			&& content_body!=NULL 
			&& content_len>=0 
			&& request->state == HTTP_PROCESS_STAT_HEADER_LINE);

	//note: dont support chunked body now, and may never in the future.
	buf_t* header_buf = request->header_buf;
	buf_t* body_buf = NULL;
	request->state = HTTP_PROCESS_STAT_CONTENT_BODY;

	if(content_len == 0)
	{
		return 1;
	}
	else if(content_len > request->conf->max_content_len)
	{
		request->status = HTTP_STATUS_BAD_REQUEST;
		return 0;
	}
	else if(content_len > header_buf->end-header_buf->pos)
	{
		size_t rest = header_buf->last - header_buf->pos;
		body_buf = buf_create(request->pool, content_len);
		if(body_buf==NULL) 
		{
			request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
			return 0;
		}
		if(rest > 0)
		{
			memcpy(body_buf->start, header_buf->pos, 
					header_buf->last - header_buf->pos);
			body_buf->last += header_buf->last - header_buf->pos;
		}
	}
	else
	{
		//the whole body is present.
		body_buf = (buf_t*) pool_alloc(request->pool, sizeof(buf_t));
		if(body_buf == NULL) 
		{
			request->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
			return 0;
		}
		body_buf->start = header_buf->pos;
		body_buf->pos = header_buf->pos;
		body_buf->last = header_buf->last;
		body_buf->end = header_buf->last;

		return 1; 
	}

	int recv_body_len = body_buf->last - body_buf->pos;
	for(;;)
	{
		if(recv_body_len >= content_len)
		{
			content_body->content = body_buf->pos;
			content_body->content_len = content_len;
			assert(content_body->content_fd < 0);
			return 1;
		}

		int count = recv(fd, body_buf->last, body_buf->end-body_buf->last, 0);
		if(count <= 0) 
		{
			request->status = HTTP_STATUS_CLOSE;
			return 0;
		}
		
		body_buf->last += count;

		recv_body_len += count;

		assert(!(recv_body_len<content_len && body_buf->end==body_buf->last));
	}

	assert(0);
	return 0;
}


