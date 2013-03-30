#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "utils.h"
#include "connection.h"
#include "pool.h"
#include "module.h"
#include "typedef.h"
#include "upstream.h"

#define eol(c) ((c) == '\n')
#define is_character(c) ((c)|0x20 >='a' && (c)|0x20 <= 'z')
#define HTTP_PARSE_DONE 1
#define HTTP_PARSE_FAIL 0
#define HTTP_PARSE_AGAIN -1

static char* find_next_eol(char* line)
{
	char* p = line;
	
	while(!eol(*p) && *p!='\0') p++;

	if(*p == '\0' || p-line >= max)
		return NULL;
	else 
		return p;
}

static int connection_parse_url(connection_t* thiz, str_t* url)
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

	char* p = url->data;
	char* url_end = url->data + url->len;
	for(p; p<url_end && *p!='\0'; p++)
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
	
	//TODO zero terminated.
	if(schema_start && schema_end)
	{
		thiz->r.url.schema.data = schema_start;
		thiz->r.url.schema.len = schema_end - schema_start;
		*schema_end = '\0';
	}
	if(host_start && host_end)
	{
		thiz->r.url.host.data = host_start;
		thiz->r.url.host.len = host_end - host_start;
		*host_end = '\0';
	}
	if(port_start && port_end)
	{
		thiz->r.url.port = strtod(port_start, &port_end);
	}
	if(path_start && path_end)
	{
		thiz->r.url.path.data = path_start;
		thiz->r.url.path.len = path_end - path_start;
		*path_end = '\0';
	}
	if(query_start && query_end)
	{
		thiz->r.url.query.data = query_start;
		thiz->r.url.query.len = query_end - query_start;
		*query_end = '\0';
	}

	return HTTP_PARSE_DONE;
}

static int connection_parse_request_line(connection_t* thiz)
{
	str_t method = {0};
	str_t url = {0};
	str_t version = {0};
	char* pos = thiz->header_buf.pos;
	char* last = thiz->header_buf.last;
	char* eol = NULL;

	//skip empty line first.
	while(isspace(*pos) && pos<last && *pos!='\0') pos++;
	thiz->header_buf.pos = pos;

	eol = find_next_eol(pos);
	if(eol == NULL)
	{
		return HTTP_PARSE_AGAIN;
	}
	
	get_token(&method, &pos, isspace, NULL);
	get_token(&url, &pos, isspace, NULL);
	get_token(&version, &pos, isspace, NULL);
	thiz->header_buf.pos = eol + 1;

	if(method.data==NULL || path.data==NULL || version.data==NULL) return HTTP_PARSE_FAIL;

	thiz->r.version_str = version;
	version.data = strstr(version, "HTTP/");
	if(version.data == NULL) return HTTP_PARSE_FAIL;
	version.data += 5;
	version.len = 3;

	if(strcmp(version.data, "1.0") == 0) thiz->r.version = HTTP_VERSION_10;
	else if(strcmp(version.data, "1.1") == 0) thiz->r.version = HTTP_VERSION_11;
	else return HTTP_PARSE_FAIL; //dont support 0.9

	if(strncasecmp(method.data, "GET", method.len) == 0) thiz->r.method = HTTP_METHOD_GET;
	else if(strncasecmp(method.data, "POST", method.len) == 0) thiz->r.method = HTTP_METHOD_POST;
	else if(strncasecmp(method.data, "HEAD", method.len) == 0) thiz->r.method = HTTP_METHOD_HEAD;
	else return HTTP_PARSE_FAIL; 

	return connection_parse_url(thiz, &url);
}

//FIXME  in one header may be multilines
static int connection_parse_header_line(connection_t* thiz)
{
	int ret = HTTP_PARSE_AGAIN;
	char* line = NULL;
	char* pos = thiz->r.header_buf.pos;
	char* last = thiz->r.header_buf.last;
	char* eol = NULL;
	str_t name = {0};
	str_t value = {0};

	while(pos<last)	
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
		//TODO should trim space.
		get_token(&name, &pos, NULL, ":");
		get_token(&value, &pos, isspace, NULL);
		if(name.data == NULL) 
		{
			ret = HTTP_PARSE_FAIL;
			break;
		}

		if(!http_header_set(&(thiz->r.headers), &name, &value))
		{
			ret = HTTP_PARSE_FAIL;
			break;
		}

		pos = eol + 1;
	}

	thiz->header_buf.pos = pos;
	if(ret == HTTP_PARSE_DONE)
	{
		//TODO we should check some crucial headers like host.
		array_t* h = &(thiz->r.headers);
		if(http_header_str(h, HTTP_HEADER_HOST) == NULL)
		{
			return HTTP_PARSE_FAIL;
		}

		thiz->r.content_len = http_header_int(h, HTTP_HEADER_CONTENT_LEN);
		if(thiz->r.content_len < 0) thiz->r.content_len = 0;

		const str_t* keep_alive = http_header_str(h, HTTP_HEADER_KEEPALIVE);
		if(keep_alive == NULL)
		{
			if(thiz->r.version == HTTP_VERSION_11)
				thiz->r.keep_alive = 1;
			else 
				thiz->r.keep_alive = 0;
		}
		else
		{
			if(strncasecmp(keep_alive->data, "CLOSE", keep_alive->len) == 0)
				thiz->r.keep_alive = 0;
			else 
				thiz->r.keep_alive = 1;
		}
	}

	return ret;
}

static int connection_alloc_large_header_buf(connection_t* thiz)
{
	buf_create(&(thiz->r.header_buf), thiz->r.pool, thiz->conf->large_client_header_size);

	if(thiz->r.header_buf.start == NULL) return 0;
	else return 1;
}

static int connection_gen_request(connection_t* thiz)
{
	enum 
	{
		HTTP_PARSE_REQUEST_LINE = 0,
		HTTP_PARSE_HEADER_LINE_LINE,
		HTTP_PARSE_BODY,
		HTTP_PARSE_DONE,
	}parse_state = HTTP_PARSE_REQUEST_LINE;

	int ret = HTTP_STATUS_OK;
	int parse_ret = HTTP_PARSE_AGAIN;

	array_init(&(thiz->r.headers), thiz->r.pool, 24);

	buf_create(&(thiz->r.header_buf), thiz->r.pool, thiz->conf->client_header_size);
	if(thiz->r.header_buf.start == NULL) 
	{
		return HTTP_STATUS_INTERNAL_SERVER_ERROR;
	}

	buf_t* buf = &(thiz->r.header_buf);
	int count = 0;

	for(;;)
	{
		//meet buffer end but still parse incomplete.
		//the last character of buffer should always be zero.
		if(buf->last == buf->end-1) 
		{
			assert(parse_state <= HTTP_PARSE_HEADER_LINE);
			if(!connection_alloc_large_header_buf(thiz))
			{
				ret = HTTP_STATUS_INTERNAL_SERVER_ERROR;
				break;
			}
		}

		count = recv(thiz->fd, buf->last, buf->end-1-buf->last);
		if(count <= 0) 
		{
			ret = HTTP_STATUS_CLOSE;
			break;
		}
		buf->last += count;

		switch(parse_state)
		{
			case HTTP_PARSE_REQUEST_LINE:
				parse_ret = connection_parse_request_line(thiz);
				if(parse_ret == HTTP_PARSE_DONE) parse_state = HTTP_PARSE_HEADER_LINE;
				break;
			case HTTP_PARSE_HEADER_LINE:
				parse_ret = connection_parse_header_line(thiz);
				if(parse_ret == HTTP_PARSE_DONE) 
				{
					parse_state = HTTP_PARSE_BODY;

					if(thiz->r.content_len > thiz->conf->max_content_len)
					{
						ret = HTTP_STATUS_BAD_REQUEST;
						break;
					}
					else if(thiz->r.content_len > buf->end-1 - buf->pos)
					{
						size_t rest = buf->last - buf->pos;
						buf_create(&(thiz->r.body_buf), thiz->r.pool, thiz->r.content_len + 1);
						if(thiz->r.body_buf.start == NULL) 
						{
							ret = HTTP_STATUS_INTERNAL_SERVER_ERROR;
							break;
						}
						if(rest > 0)
						{
							memcpy(thiz->r.body_buf.last, buf->pos, buf->last - buf->pos);
							thiz->r.body_buf.last += buf->last - buf->pos;
						}
						buf = &(thiz->r.body_buf);
					}
				}
				
				break;
			case HTTP_PARSE_BODY:
				if(buf->last - buf->pos >= thiz->r.content_len)
				{
					state = HTTP_PARSE_DONE;
				}
				break;
			case HTTP_PARSE_DONE:
			default:
				assert(0);
				break;
		}

		if(parse_ret==HTTP_PARSE_FAIL)
		{
			ret = HTTP_STATUS_BAD_REQUEST;
			break;
		}
		if(parse_state==HTTP_PARSE_DONE)
		{
			ret = HTTP_STATUS_OK;
			break;
		}
	}

	return ret;
}

static size_t connection_calculate_response_len(connection_t* thiz)
{
	size_t new_line_len = 2;
	size_t len = 0;
	assert(thiz->r.version >= HTTP_VERSION_10);
	
	len += sizeof("HTTP/1.x ") - 1;
	len += sizeof("200") - 1; //all length of status code are 3.
	len += strlen(http_status_line(thiz->r.response->status));
	len += new_line_len; 


	http_header_t** headers = (http_header_t**) thiz->r.response->headers;
	int header_count = thiz->r.response->headers.count;
	int i = 0; 
	for(i; i<header_count; i++)
	{
		len += headers[i]->name.len;
		len += headers[i]->value.len;
		len += 4; //strlen(\r\n) + ': ' after name.
	}

	len += new_line_len;

	return len;
}

//success return 1 else return 0.
static int connection_send_response(connection_t* thiz)
{
	int count = 0;
	size_t num = 0;
	http_response_t* response = thiz->r.response;
	http_header_t** headers = (http_header_t**) response->headers.elts;
	int header_count = response->headers.count;

	//TODO add some crucial headers such as: connection, date, server...
	int response_len = connection_calculate_response_len(thiz);
	char* buf = pool_alloc(thiz->r.pool, response_len);

	count = snprintf(buf, "%s %d %s\r\n", thiz->r.version_str, 
										  response->status, 
										  http_status_line(response->status));
	if(count <= 0) return 0;
	num += count;

	int i = 0;
	for(i; i<header_count; i++)
	{
		count = snprintf(buf+num, response_len-num, "%s: %s\r\n", 
								  headers[i]->name.data, headers[i]->value.data);
		if(count <= 0) return 0;
		num += count;
	}

	buf[num] = '\r';
	buf[num+1] = '\n';
	num += 2;

	assert(num == response_len);

	if(!nwrite(thiz->fd, buf, num)) return 0;

	assert((response->content_fd>=0 && response->content_body.pos==NULL)
			|| (response->content_fd<0 && response->content_body.pos!=NULL));
	if(response->content_fd >= 0)
	{
		ssize_t ret = sendfile(thiz->fd, response->content_fd, NULL, response->content_len);
		close(response->content_fd);
		repsone->content_fd = -1;

		if(ret < 0) return 0;
	}
	else if(response->content_body.pos != NULL)
	{
		if(!nwrite(thiz->fd, response->content_body.pos, response->content_len)) 
			return 0;
	}

	return 1;
}

static int connection_process_request(connection_t* thiz)
{
	vhost_conf_t** vhosts = (vhost_conf_t** )thiz->conf->vhosts.elts;
	size_t vhost_nr = thiz->conf->vhosts.count;
	size_t i = 0;
	for(; i<vhost_nr; i++)
	{
		if(strncasecmp(thiz->r.url.host.data, 
					   vhosts[i]->name, 
					   thiz->r.url.host.len) == 0)
		{
			break;
		}
	}

	if(i == vhost_nr) return HTTP_PROCESS_FAIL;
		
	vhost_loc_conf_t** locs = (vhost_loc_conf_t** ) vhosts[i]->locs.elts;
	size_t loc_nr = vhost[i]->locs.count;

	int k = 0;
	for(k; k<loc_nr; k++)
	{
		if(regexec(&(locs[k]->pattern_regex), thiz->r.url.path.data, 0, NULL, 0) == 0)
			break;
	}

	//TODO there should be a default handler.
	if(k == loc_nr) return HTTP_PROCESS_FAIL;
	
	int ret = HTTP_MODULE_PROCESS_DONE;
	ret = module_process(locs[k]->handler, &(thiz->r));

	if(ret == HTTP_MODULE_PROCESS_FAIL)
	{
		return HTTP_STATUS_BAD_REQUEST;
	}
	else if(ret == HTTP_MODULE_PROCESS_DONE)
	{
		return HTTP_STATUS_OK;
	}
	else if(ret == HTTP_MODULE_PROCESS_UPSTREAM)
	{
		ret = upstream_process(thiz->r.upstream);
		if(ret == HTTP_UPSTREAM_DONE) return HTTP_STATUS_OK;
		else return HTTP_STATUS_BAD_REQUEST;
	}
	else
	{
		assert(0);
		return HTTP_STATUS_INTERNAL_SERVER_ERROR;
	}
}

static int connection_finalize_request(connection_t* thiz)
{
	pthread_mutex_lock(&(thiz->mutex));

	if(thiz->r.upstream != NULL) 
		upstream_destroy(thiz->r.upstream);
	thiz->r.upstream = NULL;
	
	pthread_mutex_unlock(&(thiz->mutex));

	pool_t* pool = thiz->r.pool;
	pool_reset(pool);
	bzero(&(thiz->r), sizeof(http_request_t));
	thiz->r.pool = pool;

	thiz->start_time = time();
	//TODO set new timer value.

	return 1;
}

static int connection_special_response(connection_t* thiz, int status)
{
	//TODO....
	//we need an array for the status,
	//each element stores specified strings,
	//set it as the content body.
	//maybe we will change some connection attributes here, 
	//such as keepalive.
}

connection_t* connection_create(pool_t* pool, conf_t* conf)
{
	assert(pool!=NULL && conf!=NULL);
	connection_t* thiz = pool_calloc(pool, sizeof(connection_t));
	if(thiz == NULL) return NULL;
	pool_add_cleanup(pool, connection_close, thiz);
	
	pthread_mutex_init(&thiz->mutex, NULL);
	thiz->pool = pool;
	thiz->conf = conf;
	thiz->fd = -1;

	return thiz;
}

int connection_run(connection_t* thiz, int fd)
{
	assert(thiz!=NULL && fd >= 0);
	int ret = 1;

	if(thiz->running) return 0;
	assert(thiz->r.pool == NULL && thiz->fd == -1);
	thiz->r.pool = pool_create(thiz->conf->request_pool_size);
	if(thiz->r.pool == NULL) return 0;
	thiz->start_time = time();
	thiz->fd = fd;
	
	thiz->running = 1;
	while(!thiz->timedout && thiz->running)
	{
		//only this thread are permitted to change it.
		assert(thiz->running); 

		ret = connection_gen_request(thiz);
		if(ret != HTTP_STATUS_OK)
		{
			goto DONE;
		}
		if(thiz->timedout) break;
		
		ret = connection_process_request(thiz);
		//TODO should be <= HTTP_STATUS_SPECIAL_RESPONSE
		if(ret == HTTP_STATUS_OK)
		{
			if(!connection_send_response(thiz)) ret = HTTP_STATUS_CLOSE;
		}
		if(thiz->timedout) break;

DONE:
		if(ret == HTTP_STATUS_CLOSE) 
		{
			break;
		}
		else if(ret >= HTTP_STATUS_SPECIAL_RESPONSE)
		{
			connection_special_response(thiz, ret);
		}

		if(thiz->r.keep_alive && !thiz->timedout)
		{
			connection_finalize_request(thiz);
			continue;
		}
		else
		{
			break;
		}
	}
	
	thiz->running = 0;
	connection_reusable(thiz, 1);

	return ret;
}

int connection_check_timeout(connection_t* thiz)
{
	assert(thiz != NULL);
	if(thiz->running == 0) return 1;

	time_t now = time();
	if(now-thiz->start_time >= thiz->conf->connection_timeout)
	{
		pthread_mutex_lock(&thiz->mutex);
		if(thiz->running)
		{
			//close blocking process to help connection_run thread exit asap.
			thiz->timedout = 1;
			if(thiz->fd >= 0) 
			{
				close(thiz->fd);
				while(thiz->running) sleep(1);
				thiz->fd = -1;
			}
			if(thiz->r.upstream != NULL) 
			{
				upstream_destroy(thiz->r.upstream);
				while(thiz->running) sleep(1);
				thiz->r.upstream = NULL;
			}
		}
		pthread_mutex_unlock(&thiz->mutex);
	}

	return 1;
}

static int connection_reusable(connection_t* thiz, int reuseable)
{
	assert(thiz!=NULL);	

	pthread_mutex_lock(&thiz->mutex);
	thiz->timedout = 0;
	if(thiz->fd >= 0)
	{
		close(thiz->fd);
		while(thiz->running) sleep(1);
		thiz->fd = -1;
	}

	thiz->start_time = 0;
	if(thiz->r.upstream != NULL) 
	{
		upstream_destroy(thiz->r.upstream);
		while(thiz->running) sleep(1);
		thiz->r.upstream = NULL;
	}

	pool_destroy(&thiz->r.pool);
	bzero(&(thiz->r), sizeof(http_request_t));

	pthread_mutex_unlock(&thiz->mutex);
	
	if(!reuseable)
		pthread_mutex_destroy(&thiz->mutex);

	return 1;
}

void connection_close(void* ctx)
{
	connection_t* thiz = (connection_t* ) ctx;

	connection_reusable(thiz, 0);
}

