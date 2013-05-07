#ifndef HTTP_H
#define HTTP_H
#include "typedef.h"
#include "array.h"
#include "buf.h"
#include <stdint.h>
typedef struct conf_s conf_t;
typedef struct upstream_s upstream_t;
struct sockaddr_in;

typedef struct http_header_s 
{
	str_t name;
	str_t value;
}http_header_t;

typedef struct url_s
{
	str_t unparsed_url;
	str_t schema;
	str_t host;
	int port;
	str_t path;
	str_t query_string;
	//str_t fragment_id;
}url_t;

typedef enum 
{
	HTTP_METHOD_GET = 0,
	HTTP_METHOD_POST,
	HTTP_METHOD_HEAD,
}http_method_e;

typedef enum
{
	HTTP_VERSION_10 = 0,
	HTTP_VERSION_11,
}http_version_e;

typedef enum 
{
	HTTP_PROCESS_STAT_NONE = 0,
	HTTP_PROCESS_STAT_REQUEST_LINE,
	HTTP_PROCESS_STAT_HEADER_LINE,
	HTTP_PROCESS_STAT_CONTENT_BODY,
	HTTP_PROCESS_STAT_STATUS_LINE,
}http_process_state_e;

typedef struct http_headers_in_s
{
	array_t* headers; //http_header_t* 

	const str_t* header_host;
	const str_t* header_content_type;
	const str_t* header_content_len;

	int content_len;
}http_headers_in_t;

typedef struct http_headers_out_s
{
	array_t* headers;

	//TODO read nginx code here.
	int content_len;
}http_headers_out_t;

typedef struct http_content_body_s
{
	int content_len;
	buf_t* content;	
	int content_fd;
}http_content_body_t;

typedef struct http_request_s
{
	pool_t* pool;
	const conf_t* conf;

	struct sockaddr_in* peer_addr;
	str_t remote_ip;
	uint16_t remote_port;
	http_process_state_e state;
	str_t method_str;
	http_method_e method;
	url_t url;
	str_t version_str;
	http_version_e version;
	int status; //response status.
	int keep_alive;

	buf_t* header_buf;
	http_headers_in_t headers_in;
	http_headers_out_t headers_out;

	http_content_body_t body_in;
	http_content_body_t body_out;

	upstream_t* upstream;
}http_request_t;

#define HTTP_STATUS_START                     100
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
#define HTTP_STATUS_END                       599 

extern const str_t* HTTP_HEADER_CONNECTION;
extern const str_t* HTTP_HEADER_DATE;
extern const str_t* HTTP_HEADER_SERVER;
extern const str_t* HTTP_HEADER_HOST;
extern const str_t* HTTP_HEADER_CONTENT_TYPE;
extern const str_t* HTTP_HEADER_CONTENT_LEN;
extern const str_t* HTTP_HEADER_KEEPALIVE;
extern const str_t* HTTP_HEADER_CLOSE;
extern const str_t* HTTP_HEADER_XWP_VER;

//TODO char* to str_t* 
int http_content_type(const char* extension, str_t* content_type);
const str_t* http_status_line(int status);
int http_header_set(array_t* headers, const str_t* name, const str_t* value);
int http_header_equal(array_t* headers, const str_t* name, const str_t* value);
const str_t* http_header_str(array_t* headers, const str_t* name);
int http_header_int(array_t* headers, const str_t* name);
str_t* http_error_page(int status, pool_t* pool);

#define HTTP_PROCESS_PHASE_REQUEST  0
#define HTTP_PROCESS_PHASE_RESPONSE 1 
http_request_t* http_request_create(pool_t* pool, const conf_t* conf, struct sockaddr_in* peer_addr);
int http_process_request_line(http_request_t* request, int fd);
int http_process_header_line(http_request_t* request, int fd, int process_phase);
int http_process_status_line(http_request_t* request, int fd);
int http_process_content_body(http_request_t* request, int fd, http_content_body_t* content_body, int content_len);

#endif
