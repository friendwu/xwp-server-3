#include <ctype.h>
#include <string.h>
#include "http.h"
#include "utils.h"

#define eol(c) ((c) == '\n')
#define is_character(c) (((c)|0x20) >='a' && ((c)|0x20) <= 'z')

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
	assert(url!=NULL && url_str!=NULL);

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

int http_parse_request_line(http_request_t* request, buf_t* buf)
{
	assert(request!=NULL && buf!=NULL);

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
	
	get_token(&method, &pos, isspace, NULL);
	get_token(&url, &pos, isspace, NULL);
	get_token(&version, &pos, isspace, NULL);
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
int http_parse_header_line(http_request_t* request, buf_t* buf, array_t* headers)
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
		get_token(&value, &pos, isspace, NULL);
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

int http_parse_content_body(http_request_t* request, buf_t* buf, int chunked, int content_len)
{
	assert(request!=NULL && buf!=NULL && !(chunked==0 && content_len<0));

	if(chunked)
	{
		//TODO.
		return HTTP_PARSE_DONE;
	}
	else
	{
		if(buf->last - buf->pos >= content_len)
		{
			return HTTP_PARSE_DONE;
		}
		else
		{
			return HTTP_PARSE_AGAIN;
		}
	}
}

int http_parse_status_line(http_request_t* request, buf_t* buf, int* status)
{
	return HTTP_PARSE_DONE;
}

