#ifndef TYPEDEF_H
#define TYPEDEF_H
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

#define DESTROY_MEM(func, p) if(!(p)) \
	{func(p);} \
	
#define SAFE_FREE(p) if(p != NULL) {free(p); p = NULL;}

#define MAX_ATTR_NR 64
typedef struct array_s
{
	pool_t* pool;
	void** elts;
	int count;
	int nalloc;
}array_t;

typedef struct buf_s
{
	char* start;
	char* end;
	char* pos;
	char* last;
}buf_t;

typedef struct str_s
{
	char* data; //should be zero terminated.
	size_t len;
}str_t;

typedef struct http_header_s 
{
	str_t name;
	str_t value;
}http_header_t;

typedef struct url_s
{
	str_t schema;
	str_t host;
	int port;
	str_t path;
	st_t query_string;
	str_t fragment_id;
}url_t;

typedef enum 
{
	METHOD_GET = 0,
	METHOD_POST,
	METHOD_HEAD,
}http_method_e;

typedef struct http_request_s
{
	pool_t* pool;
	http_method_e method;
	url_t url;
	http_version_e version;
	array_t headers; //http_header_t* 
	buf_t header_buf;
	buf_t body_buf;
	int keep_alive;
	str_t usragent;
	int content_length;
	upstream_t* upstream;

	http_response_t* response;
}http_request_t;

typedef struct http_response_s
{
	int status;
	array_t headers; //http_header_t* 
	buf_t content_body;
	int content_fd;
}http_response_t;

#define HTTP_STATUS_OK                        200
#define HTTP_STATUS_CREATED                   201
#define HTTP_STATUS_ACCEPTED                  202
#define HTTP_STATUS_NO_CONTENT                204
#define HTTP_STATUS_PARTIAL_CONTENT           206

#define HTTP_STATUS_SPECIAL_RESPONSE          300
#define HTTP_STATUS_MOVED_PERMANENTLY         301
#define HTTP_STATUS_MOVED_TEMPORARILY         302
#define HTTP_STATUS_SEE_OTHER                 303
#define HTTP_STATUS_NOT_MODIFIED              304
#define HTTP_STATUS_TEMPORARY_REDIRECT        307

#define HTTP_STATUS_BAD_REQUEST               400
#define HTTP_STATUS_UNAUTHORIZED              401
#define HTTP_STATUS_FORBIDDEN                 403
#define HTTP_STATUS_NOT_FOUND                 404
#define HTTP_STATUS_NOT_ALLOWED               405
#define HTTP_STATUS_REQUEST_TIME_OUT          408
#define HTTP_STATUS_CONFLICT                  409
#define HTTP_STATUS_LENGTH_REQUIRED           411
#define HTTP_STATUS_PRECONDITION_FAILED       412
#define HTTP_STATUS_REQUEST_ENTITY_TOO_LARGE  413
#define HTTP_STATUS_REQUEST_URI_TOO_LARGE     414
#define HTTP_STATUS_UNSUPPORTED_MEDIA_TYPE    415
#define HTTP_STATUS_RANGE_NOT_SATISFIABLE     416

/* The special code to close connection without any response */
#define HTTP_STATUS_CLOSE                     444

#define HTTP_STATUS_NGINX_CODES               494

#define HTTP_STATUS_REQUEST_HEADER_TOO_LARGE  494

#define HTTPS_CERT_ERROR               495
#define HTTPS_NO_CERT                  496

/*
 * We use the special code for the plain HTTP requests that are sent to
 * HTTPS port to distinguish it from 4XX in an error page redirection
 */
#define HTTP_STATUS_TO_HTTPS                  497

/* 498 is the canceled code for the requests with invalid host name */

/*
 * HTTP does not define the code for the case when a client closed
 * the connection while we are processing its request so we introduce
 * own code to log such situation when a client has closed the connection
 * before we even try to send the HTTP header to it
 */
#define HTTP_STATUS_CLIENT_CLOSED_REQUEST     499


#define HTTP_STATUS_INTERNAL_SERVER_ERROR     500
#define HTTP_STATUS_NOT_IMPLEMENTED           501
#define HTTP_STATUS_BAD_GATEWAY               502
#define HTTP_STATUS_SERVICE_UNAVAILABLE       503
#define HTTP_STATUS_GATEWAY_TIME_OUT          504
#define HTTP_STATUS_INSUFFICIENT_STORAGE      507

#define HTTP_HEADER_CONNECTION "Connection"
#define HTTP_HEADER_DATE "Date"
#define HTTP_HEADER_SERVER "Server"
#define HTTP_HEADER_HOST "Host"
#define HTTP_HEADER_CONTENT_TYPE "Content-Type"
#define HTTP_HEADER_CONTENT_LEN "Content-Len"
#define HTTP_HEADER_KEEPALIVE "Keep-alive"

#endif/*TYPEDEF_H*/

