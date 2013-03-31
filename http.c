#include "http.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const str_t http_header_names[] = 
{
	server_string("Connection"),
	server_string("Date"),
	server_string("Server"),
	server_string("Host"),
	server_string("Content-Type"),
	server_string("Content-Len"),
	server_string("Keep-alive"),
}

const str_t* HTTP_HEADER_CONNECTION = &http_header_names[0];
const str_t* HTTP_HEADER_DATE = &http_header_names[1];
const str_t* HTTP_HEADER_SERVER = &http_header_names[2];
const str_t* HTTP_HEADER_HOST = &http_header_names[3];
const str_t* HTTP_HEADER_CONTENT_TYPE = &http_header_names[4];
const str_t* HTTP_HEADER_CONTENT_LEN = &http_header_names[5];
const str_t* HTTP_HEADER_KEEPALIVE = &http_header_names[6];

typedef struct http_status_s 
{
	int status;
	str_t line;
}http_status_t;

static http_status_t http_status_infos[] = {
	{200, server_string("200 OK")},
	{201, server_string("201 Created")},
	{202, server_string("202 Accepted")},
	//ngx_null_string,  /* "203 Non-Authoritative Information" */
	{204, server_string("204 No Content")},
	//ngx_null_string,  /* "205 Reset Content" */
	{206, server_string("206 Partial Content")},

	/* //ngx_null_string, */  /* "207 Multi-Status" */

	//#define NGX_HTTP_LAST_2XX  207
	//#define NGX_HTTP_OFF_3XX   (NGX_HTTP_LAST_2XX - 200)

	/* //ngx_null_string, */  /* "300 Multiple Choices" */

	{301, server_string("301 Moved Permanently")},
	{302, server_string("302 Moved Temporarily")},
	{303, server_string("303 See Other")},
	{304, server_string("304 Not Modified")},
	//ngx_null_string,  /* "305 Use Proxy" */
	//ngx_null_string,  /* "306 unused" */
	{307, server_string("307 Temporary Redirect")},

	//#define NGX_HTTP_LAST_3XX  308
	//#define NGX_HTTP_OFF_4XX   (NGX_HTTP_LAST_3XX - 301 + NGX_HTTP_OFF_3XX)

	{400, server_string("400 Bad Request")},
	{401, server_string("401 Unauthorized")},
	{402, server_string("402 Payment Required")},
	{403, server_string("403 Forbidden")},
	{404, server_string("404 Not Found")},
	{405, server_string("405 Not Allowed")},
	{406, server_string("406 Not Acceptable")},
	//ngx_null_string,  /* "407 Proxy Authentication Required" */
	{408, server_string("408 Request Time-out")},
	{409, server_string("409 Conflict")},
	{410, server_string("410 Gone")},
	{411, server_string("411 Length Required")},
	{412, server_string("412 Precondition Failed")},
	{413, server_string("413 Request Entity Too Large")},
	//ngx_null_string,  /* "414 Request-URI Too Large", but we never send it
					   * because we treat such requests as the HTTP/0.9
					   * requests and send only a body without a header
					   */
	{415, server_string("415 Unsupported Media Type")},
	{416, server_string("416 Requested Range Not Satisfiable")},

	/* //ngx_null_string, */  /* "417 Expectation Failed" */
	/* //ngx_null_string, */  /* "418 unused" */
	/* //ngx_null_string, */  /* "419 unused" */
	/* //ngx_null_string, */  /* "420 unused" */
	/* //ngx_null_string, */  /* "421 unused" */
	/* //ngx_null_string, */  /* "422 Unprocessable Entity" */
	/* //ngx_null_string, */  /* "423 Locked" */
	/* //ngx_null_string, */  /* "424 Failed Dependency" */

	//#define NGX_HTTP_LAST_4XX  417
	//#define NGX_HTTP_OFF_5XX   (NGX_HTTP_LAST_4XX - 400 + NGX_HTTP_OFF_4XX)

	{500, server_string("500 Internal Server Error")},
	{501, server_string("501 Method Not Implemented")},
	{502, server_string("502 Bad Gateway")},
	{503, server_string("503 Service Temporarily Unavailable")},
	{504, server_string("504 Gateway Time-out")},

	//ngx_null_string,        /* "505 HTTP Version Not Supported" */
	//ngx_null_string,        /* "506 Variant Also Negotiates" */
	{507, server_string("507 Insufficient Storage")},
	/* //ngx_null_string, */  /* "508 unused" */
	/* //ngx_null_string, */  /* "509 unused" */
	/* //ngx_null_string, */  /* "510 Not Extended" */

	//#define NGX_HTTP_LAST_5XX  508
};

const str_t* http_status_line(int status)
{
	int low = 0;
	int high = sizeof(http_status_infos) / sizeof(HttpStatusInfo);
	int mid = (low + high) / 2;

	while(low <= high)
	{
		if(http_status_infos[mid].status == status) 
			return &http_status_infos[mid].line;
		else if(http_status_infos[mid].status < status) 
		{
			low = mid + 1;
		}
		else if(http_status_infos[mid].status > status) 
		{
			high = mid - 1;
		}
	}
	
	assert(0);
	return NULL;
}

const str_t HCT = server_string("text/html");
const str_t* http_content_type(const char* extension)
{
	return &HCT;
/*
	if(extension == NULL) return "text/html";
	else if(strcmp(extension, "") == 0) return "application/octet-stream";
	else if(strcmp(extension, "exe") == 0) return "application/octet-stream";
	else if(strcmp(extension, "js") == 0) return "text/html";
	else if(strcmp(extension, "gif") == 0) return "image/gif";
	else if(strcmp(extension, "bmp") == 0) return "image/x-xbitmap";
	else if(strcmp(extension, "jpg") == 0) return "image/jpeg";
	else if(strcmp(extension, "png") == 0) return "image/png";
	else if(strcmp(extension, "ico") == 0) return "image/x-icon";
	else if(strcmp(extension, "htm") == 0) return "text/html";
	else if(strcmp(extension, "asp") == 0) return "text/html";
	else if(strcmp(extension, "php") == 0) return "text/html";
	else if(strcmp(extension, "html") == 0) return "text/html";
	else if(strcmp(extension, "mht") == 0) return "text/html";
	else if(strcmp(extension, "xml") == 0) return "text/xml";
	else if(strcmp(extension, "txt") == 0) return "text/plain";
	else if(strcmp(extension, "c") == 0) return "text/plain";
	else if(strcmp(extension, "cpp") == 0) return "text/plain";
	else if(strcmp(extension, "hpp") == 0) return "text/plain";
	else if(strcmp(extension, "h") == 0) return "text/plain";
	else if(strcmp(extension, "lrc") == 0) return "text/plain";
	else if(strcmp(extension, "pdf") == 0) return "application/pdf";
	else if(strcmp(extension, "avi") == 0) return "video/avi";
	else if(strcmp(extension, "css") == 0) return "text/css";
	else if(strcmp(extension, "swf") == 0) return "application/x-shockwave-flash";
	else if(strcmp(extension, "flv") == 0) return "application/x-shockwave-flash";
	else if(strcmp(extension, "xls") == 0) return "application/vnd.ms-excel";
	else if(strcmp(extension, "doc") == 0) return "application/vnd.ms-word";
	else if(strcmp(extension, "mid") == 0) return "audio/midi";
	else if(strcmp(extension, "mp3") == 0) return "audio/mpeg";
	else if(strcmp(extension, "ogg") == 0) return "audio/ogg";
	else if(strcmp(extension, "rm") == 0) return "application/vnd.rn-realmedia";
	else if(strcmp(extension, "rmvb") == 0) return "application/vnd.rn-realmedia";
	else if(strcmp(extension, "wav") == 0) return "audio/wav";
	else if(strcmp(extension, "wmv") == 0) return "video/x-ms-wmv";
	else if(strcmp(extension, "zip") == 0) return "application/x-tar";
	else if(strcmp(extension, "rar") == 0) return "application/x-tar";
	else if(strcmp(extension, "7z") == 0) return "application/x-tar";
	else if(strcmp(extension, "tar") == 0) return "application/x-tar";
	else if(strcmp(extension, "gz") == 0) return "application/x-tar";
	else return "text/html";
	*/
}

int http_header_set(array_t* headers, const str_t* name, const str_t* value)
{
	assert(headers!=NULL && name!=NULL && value!=NULL);
	
	http_header_t* h = (http_header_t* )headers->elts;
	int i = 0;
	for(; i<headers->count; i++)
	{
		if(strncmp(name.data, h[i].name.data, name.len) == 0)
		{
			h[i].value = *value;
			return 1;
		}
	}
	http_header_t* h = pool_alloc(headers->pool, sizeof(http_header_t));

	h[i].name = *name;
	h[i].value = *value;

	array_push(headers, h);

	return 1;
}

const str_t* http_header_str(array_t* headers, const str_t* name)
{
	assert(headers!=NULL && name!=NULL);

	int i = 0;
	for(; i<headers->count; i++)
	{
		if(strncasecmp(name.data, h[i].name.data, name.len) == 0)
			return &h[i].value;
	}

	return NULL;
}

int http_header_int(array_t* headers, const str_t* name)
{
	const str_t* value = http_header_str(headers, name);

	return atoi(value.data);
}

int http_header_equal(array_t* headers, const str_t* name, const str_t* value)
{
	const str_t* header_value = http_header_str(h, name);

	if(header_value != NULL && strncasecmp(header_value.data, value, header_value.len) == 0) 
		return 1;
	else 
		return 0;
}



