#include "http.h"
#include "conf.h"
#include <string.h>
#define CRLF "\r\n"

static const str_t http_header_names[] = 
{
	server_string("Connection"),
	server_string("Date"),
	server_string("Server"),
	server_string("Host"),
	server_string("Content-Type"),
	server_string("Content-Length"),
	server_string("Keep-Alive"),
	server_string("Close"),
	server_string(XWP_SERVER_VER),
};

const str_t* HTTP_HEADER_CONNECTION = &http_header_names[0];
const str_t* HTTP_HEADER_DATE = &http_header_names[1];
const str_t* HTTP_HEADER_SERVER = &http_header_names[2];
const str_t* HTTP_HEADER_HOST = &http_header_names[3];
const str_t* HTTP_HEADER_CONTENT_TYPE = &http_header_names[4];
const str_t* HTTP_HEADER_CONTENT_LEN = &http_header_names[5];
const str_t* HTTP_HEADER_KEEPALIVE = &http_header_names[6];
const str_t* HTTP_HEADER_CLOSE = &http_header_names[7];
const str_t* HTTP_HEADER_XWP_VER = &http_header_names[8];

typedef struct http_status_s 
{
	int status;
	str_t line;
}http_status_t;

static http_status_t http_status_infos[] = {
	{200, server_string("200 OK")},
	{201, server_string("201 Created")},
	{202, server_string("202 Accepted")},
	{204, server_string("204 No Content")},
	{206, server_string("206 Partial Content")},
	{301, server_string("301 Moved Permanently")},
	{302, server_string("302 Moved Temporarily")},
	{303, server_string("303 See Other")},
	{304, server_string("304 Not Modified")},
	{307, server_string("307 Temporary Redirect")},
	{400, server_string("400 Bad Request")},
	{401, server_string("401 Unauthorized")},
	{402, server_string("402 Payment Required")},
	{403, server_string("403 Forbidden")},
	{404, server_string("404 Not Found")},
	{405, server_string("405 Not Allowed")},
	{406, server_string("406 Not Acceptable")},
	{408, server_string("408 Request Time-out")},
	{409, server_string("409 Conflict")},
	{410, server_string("410 Gone")},
	{411, server_string("411 Length Required")},
	{412, server_string("412 Precondition Failed")},
	{413, server_string("413 Request Entity Too Large")},
	{415, server_string("415 Unsupported Media Type")},
	{416, server_string("416 Requested Range Not Satisfiable")},
	{500, server_string("500 Internal Server Error")},
	{501, server_string("501 Method Not Implemented")},
	{502, server_string("502 Bad Gateway")},
	{503, server_string("503 Service Temporarily Unavailable")},
	{504, server_string("504 Gateway Time-out")},
	{507, server_string("507 Insufficient Storage")},
};

static char http_error_full_tail[] =
"<hr><center>" XWP_SERVER_VER "</center>" CRLF
"</body>" CRLF
"</html>" CRLF
;

/*
static char http_error_tail[] =
"<hr><center>nginx</center>" CRLF
"</body>" CRLF
"</html>" CRLF
;


static char http_msie_padding[] =
"<!-- a padding to disable MSIE and Chrome friendly error page -->" CRLF
"<!-- a padding to disable MSIE and Chrome friendly error page -->" CRLF
"<!-- a padding to disable MSIE and Chrome friendly error page -->" CRLF
"<!-- a padding to disable MSIE and Chrome friendly error page -->" CRLF
"<!-- a padding to disable MSIE and Chrome friendly error page -->" CRLF
"<!-- a padding to disable MSIE and Chrome friendly error page -->" CRLF
;


static char http_msie_refresh_head[] =
"<html><head><meta http-equiv=\"Refresh\" content=\"0; URL=";


static char http_msie_refresh_tail[] =
"\"></head><body></body></html>" CRLF;
*/

static char http_error_301_page[] =
"<html>" CRLF
"<head><title>301 Moved Permanently</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>301 Moved Permanently</h1></center>" CRLF
;


static char http_error_302_page[] =
"<html>" CRLF
"<head><title>302 Found</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>302 Found</h1></center>" CRLF
;


static char http_error_303_page[] =
"<html>" CRLF
"<head><title>303 See Other</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>303 See Other</h1></center>" CRLF
;


static char http_error_307_page[] =
"<html>" CRLF
"<head><title>307 Temporary Redirect</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>307 Temporary Redirect</h1></center>" CRLF
;


static char http_error_400_page[] =
"<html>" CRLF
"<head><title>400 Bad Request</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
;


static char http_error_401_page[] =
"<html>" CRLF
"<head><title>401 Authorization Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>401 Authorization Required</h1></center>" CRLF
;


static char http_error_402_page[] =
"<html>" CRLF
"<head><title>402 Payment Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>402 Payment Required</h1></center>" CRLF
;


static char http_error_403_page[] =
"<html>" CRLF
"<head><title>403 Forbidden</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>403 Forbidden</h1></center>" CRLF
;


static char http_error_404_page[] =
"<html>" CRLF
"<head><title>404 Not Found</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>404 Not Found</h1></center>" CRLF
;


static char http_error_405_page[] =
"<html>" CRLF
"<head><title>405 Not Allowed</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>405 Not Allowed</h1></center>" CRLF
;


static char http_error_406_page[] =
"<html>" CRLF
"<head><title>406 Not Acceptable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>406 Not Acceptable</h1></center>" CRLF
;


static char http_error_408_page[] =
"<html>" CRLF
"<head><title>408 Request Time-out</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>408 Request Time-out</h1></center>" CRLF
;


static char http_error_409_page[] =
"<html>" CRLF
"<head><title>409 Conflict</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>409 Conflict</h1></center>" CRLF
;


static char http_error_410_page[] =
"<html>" CRLF
"<head><title>410 Gone</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>410 Gone</h1></center>" CRLF
;


static char http_error_411_page[] =
"<html>" CRLF
"<head><title>411 Length Required</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>411 Length Required</h1></center>" CRLF
;


static char http_error_412_page[] =
"<html>" CRLF
"<head><title>412 Precondition Failed</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>412 Precondition Failed</h1></center>" CRLF
;


static char http_error_413_page[] =
"<html>" CRLF
"<head><title>413 Request Entity Too Large</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>413 Request Entity Too Large</h1></center>" CRLF
;


static char http_error_414_page[] =
"<html>" CRLF
"<head><title>414 Request-URI Too Large</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>414 Request-URI Too Large</h1></center>" CRLF
;


static char http_error_415_page[] =
"<html>" CRLF
"<head><title>415 Unsupported Media Type</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>415 Unsupported Media Type</h1></center>" CRLF
;


static char http_error_416_page[] =
"<html>" CRLF
"<head><title>416 Requested Range Not Satisfiable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>416 Requested Range Not Satisfiable</h1></center>" CRLF
;


static char http_error_494_page[] =
"<html>" CRLF
"<head><title>400 Request Header Or Cookie Too Large</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>Request Header Or Cookie Too Large</center>" CRLF
;


static char http_error_495_page[] =
"<html>" CRLF
"<head><title>400 The SSL certificate error</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>The SSL certificate error</center>" CRLF
;


static char http_error_496_page[] =
"<html>" CRLF
"<head><title>400 No required SSL certificate was sent</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>No required SSL certificate was sent</center>" CRLF
;


static char http_error_497_page[] =
"<html>" CRLF
"<head><title>400 The plain HTTP request was sent to HTTPS port</title></head>"
CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>400 Bad Request</h1></center>" CRLF
"<center>The plain HTTP request was sent to HTTPS port</center>" CRLF
;


static char http_error_500_page[] =
"<html>" CRLF
"<head><title>500 Internal Server Error</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>500 Internal Server Error</h1></center>" CRLF
;


static char http_error_501_page[] =
"<html>" CRLF
"<head><title>501 Method Not Implemented</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>501 Method Not Implemented</h1></center>" CRLF
;


static char http_error_502_page[] =
"<html>" CRLF
"<head><title>502 Bad Gateway</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>502 Bad Gateway</h1></center>" CRLF
;


static char http_error_503_page[] =
"<html>" CRLF
"<head><title>503 Service Temporarily Unavailable</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>503 Service Temporarily Unavailable</h1></center>" CRLF
;


static char http_error_504_page[] =
"<html>" CRLF
"<head><title>504 Gateway Time-out</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>504 Gateway Time-out</h1></center>" CRLF
;


static char http_error_507_page[] =
"<html>" CRLF
"<head><title>507 Insufficient Storage</title></head>" CRLF
"<body bgcolor=\"white\">" CRLF
"<center><h1>507 Insufficient Storage</h1></center>" CRLF
;
typedef struct http_error_page_s
{
	int status;
	str_t page;
}http_error_page_t;

static http_error_page_t http_error_pages[] = {
	{301, server_string(http_error_301_page)},
    {302, server_string(http_error_302_page)},
    {303, server_string(http_error_303_page)},
    {307, server_string(http_error_307_page)},
    {400, server_string(http_error_400_page)},
    {401, server_string(http_error_401_page)},
    {402, server_string(http_error_402_page)},
    {403, server_string(http_error_403_page)},
    {404, server_string(http_error_404_page)},
    {405, server_string(http_error_405_page)},
    {406, server_string(http_error_406_page)},
    {408, server_string(http_error_408_page)},
    {409, server_string(http_error_409_page)},
    {410, server_string(http_error_410_page)},
    {411, server_string(http_error_411_page)},
    {412, server_string(http_error_412_page)},
    {413, server_string(http_error_413_page)},
    {414, server_string(http_error_414_page)},
    {415, server_string(http_error_415_page)},
    {416, server_string(http_error_416_page)},
    {494, server_string(http_error_494_page)}, /* 494, request header too large */
    {495, server_string(http_error_495_page)}, /* 495, https certificate error */
    {496, server_string(http_error_496_page)}, /* 496, https no certificate */
    {497, server_string(http_error_497_page)}, /* 497, http to https */
    {404, server_string(http_error_404_page)}, /* 498, canceled */
    {500, server_string(http_error_500_page)},
    {501, server_string(http_error_501_page)},
    {502, server_string(http_error_502_page)},
    {503, server_string(http_error_503_page)},
    {504, server_string(http_error_504_page)},
    {507, server_string(http_error_507_page)},
};


const str_t* http_status_line(int status)
{
	int low = 0;
	int high = sizeof(http_status_infos) / sizeof(http_status_t);

	while(low <= high)
	{
		int mid = (low + high) / 2;
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
	
	//TODO
	assert(0);
	return NULL;
}

//TODO to be optimized.
int http_content_type(const char* extension, str_t* content_type)
{
	if(extension == NULL) { to_string2(content_type, "text/html"); }
	else if(strcmp(extension, "") == 0) { to_string2(content_type, "application/octet-stream"); }
	else if(strcmp(extension, "exe") == 0) { to_string2(content_type, "application/octet-stream"); }
	else if(strcmp(extension, "js") == 0) { to_string2(content_type, "text/html"); }
	else if(strcmp(extension, "gif") == 0) { to_string2(content_type, "image/gif"); }
	else if(strcmp(extension, "bmp") == 0) { to_string2(content_type, "image/x-xbitmap"); }
	else if(strcmp(extension, "jpg") == 0) { to_string2(content_type, "image/jpeg"); }
	else if(strcmp(extension, "png") == 0) { to_string2(content_type, "image/png"); }
	else if(strcmp(extension, "ico") == 0) { to_string2(content_type, "image/x-icon"); }
	else if(strcmp(extension, "htm") == 0) { to_string2(content_type, "text/html"); }
	else if(strcmp(extension, "asp") == 0) { to_string2(content_type, "text/html"); }
	else if(strcmp(extension, "php") == 0) { to_string2(content_type, "text/html"); }
	else if(strcmp(extension, "html") == 0) { to_string2(content_type, "text/html"); }
	else if(strcmp(extension, "mht") == 0) { to_string2(content_type, "text/html"); }
	else if(strcmp(extension, "xml") == 0) { to_string2(content_type, "text/xml"); }
	else if(strcmp(extension, "txt") == 0) { to_string2(content_type, "text/plain"); }
	else if(strcmp(extension, "c") == 0) { to_string2(content_type, "text/plain"); }
	else if(strcmp(extension, "cpp") == 0) { to_string2(content_type, "text/plain"); }
	else if(strcmp(extension, "hpp") == 0) { to_string2(content_type, "text/plain"); }
	else if(strcmp(extension, "h") == 0) { to_string2(content_type, "text/plain"); }
	else if(strcmp(extension, "lrc") == 0) { to_string2(content_type, "text/plain"); }
	else if(strcmp(extension, "pdf") == 0) { to_string2(content_type, "application/pdf"); }
	else if(strcmp(extension, "avi") == 0) { to_string2(content_type, "video/avi"); }
	else if(strcmp(extension, "css") == 0) { to_string2(content_type, "text/css"); }
	else if(strcmp(extension, "swf") == 0) { to_string2(content_type, "application/x-shockwave-flash"); }
	else if(strcmp(extension, "flv") == 0) { to_string2(content_type, "application/x-shockwave-flash"); }
	else if(strcmp(extension, "xls") == 0) { to_string2(content_type, "application/vnd.ms-excel"); }
	else if(strcmp(extension, "doc") == 0) { to_string2(content_type, "application/vnd.ms-word"); }
	else if(strcmp(extension, "mid") == 0) { to_string2(content_type, "audio/midi"); }
	else if(strcmp(extension, "mp3") == 0) { to_string2(content_type, "audio/mpeg"); }
	else if(strcmp(extension, "ogg") == 0) { to_string2(content_type, "audio/ogg"); }
	else if(strcmp(extension, "rm") == 0) { to_string2(content_type, "application/vnd.rn-realmedia"); }
	else if(strcmp(extension, "rmvb") == 0) { to_string2(content_type, "application/vnd.rn-realmedia"); }
	else if(strcmp(extension, "wav") == 0) { to_string2(content_type, "audio/wav"); }
	else if(strcmp(extension, "wmv") == 0) { to_string2(content_type, "video/x-ms-wmv"); }
	else if(strcmp(extension, "zip") == 0) { to_string2(content_type, "application/x-tar"); }
	else if(strcmp(extension, "rar") == 0) { to_string2(content_type, "application/x-tar"); }
	else if(strcmp(extension, "7z") == 0) { to_string2(content_type, "application/x-tar"); }
	else if(strcmp(extension, "tar") == 0) { to_string2(content_type, "application/x-tar"); }
	else if(strcmp(extension, "gz") == 0) { to_string2(content_type, "application/x-tar"); }
	else { to_string2(content_type, "text/html"); }//TODO add log here.

	return 1;
}

int http_header_set(array_t* headers, const str_t* name, const str_t* value)
{
	assert(headers!=NULL && name!=NULL && value!=NULL);
	
	http_header_t** harray = (http_header_t** )headers->elts;
	int i = 0;
	for(; i<headers->count; i++)
	{
		if(strncmp(name->data, harray[i]->name.data, name->len) == 0)
		{
			harray[i]->value = *value;
			return 1;
		}
	}
	http_header_t* h = (http_header_t* )pool_alloc(headers->pool, sizeof(http_header_t));

	h->name = *name;
	h->value = *value;

	array_push(headers, h);

	return 1;
}

const str_t* http_header_str(array_t* headers, const str_t* name)
{
	assert(headers!=NULL && name!=NULL);

	http_header_t** harray = (http_header_t** )headers->elts;
	int i = 0;
	for(; i<headers->count; i++)
	{
		if(strncasecmp(name->data, harray[i]->name.data, name->len) == 0)
			return &harray[i]->value;
	}

	return NULL;
}

int http_header_int(array_t* headers, const str_t* name)
{
	const str_t* value = http_header_str(headers, name);
	if(value == NULL) return -1;

	return atoi(value->data);
}

int http_header_equal(array_t* headers, const str_t* name, const str_t* value)
{
	const str_t* header_value = http_header_str(headers, name);

	if(header_value != NULL && strncasecmp(header_value->data, value->data, header_value->len) == 0) 
		return 1;
	else 
		return 0;
}

str_t* http_error_page(int status, pool_t* pool)
{
	int low = 0;
	int high = sizeof(http_error_pages) / sizeof(http_error_page_t);
	str_t* page = NULL;

	while(low <= high)
	{
		int mid = (low + high) / 2;
		if(http_error_pages[mid].status == status) 
		{
			page = &http_error_pages[mid].page;
			break;
		}
		else if(http_error_pages[mid].status < status) 
		{
			low = mid + 1;
		}
		else if(http_error_pages[mid].status > status) 
		{
			high = mid - 1;
		}
	}

	if(page == NULL) return NULL;
	
	str_t* ret_page = pool_alloc(pool, sizeof(str_t));
	if(ret_page == NULL) return NULL;
	ret_page->data = pool_alloc(pool, page->len + sizeof(http_error_full_tail) - 1 + 1);
	if(ret_page->data == NULL) return NULL;

	strncpy(ret_page->data, page->data, page->len);
	strncpy(ret_page->data+page->len, http_error_full_tail, sizeof(http_error_full_tail) - 1);
	ret_page->len = page->len + sizeof(http_error_full_tail) - 1;

	return ret_page;
}


