#include <unistd.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <strings.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include "utils.h"
#include "pool.h"

#define IS_DELIM(delim_func, s, c) ((delim_func!=NULL && delim_func(c)) || (s!=NULL && strchr(s, c)!=NULL))
typedef struct http_status_s 
{
	int status;
	const char* line;
	const char* verbose;
}http_status_t;

static http_status_t http_status_infos[] = {
	 {100, "Continue", "Request received, please continue"},
	 {101, "Switching Protocols",
			 "Switching to new protocol; obey Upgrade header"},

	 {200, "OK", "Request fulfilled, document follows"},
	 {201, "Created", "Document created, URL follows"},
	 {202, "Accepted",
			 "Request accepted, processing continues off-line"},
	 {203, "Non-Authoritative Information", "Request fulfilled from cache"},
	 {204, "No Content", "Request fulfilled, nothing follows"},
	 {205, "Reset Content", "Clear input form for further input."},
	 {206, "Partial Content", "Partial content follows."},

	 {300, "Multiple Choices",
			 "Object has several resources -- see URI list"},
	 {301, "Moved Permanently", "Object moved permanently -- see URI list"},
	 {302, "Found", "Object moved temporarily -- see URI list"},
	 {303, "See Other", "Object moved -- see Method and URL list"},
	 {304, "Not Modified",
			 "Document has not changed since given time"},
	 {305, "Use Proxy",
			 "You must use proxy specified in Location to access this "
			 "resource."},
	 {307, "Temporary Redirect",
			 "Object moved temporarily -- see URI list"},

	 {400, "Bad Request",
			 "Bad request syntax or unsupported method"},
	 {401, "Unauthorized",
			 "No permission -- see authorization schemes"},
	 {402, "Payment Required",
			 "No payment -- see charging schemes"},
	 {403, "Forbidden",
			 "Request forbidden -- authorization will not help"},
	 {404, "Not Found", "Nothing matches the given URI"},
	 {405, "Method Not Allowed",
			 "Specified method is invalid for this resource."},
	 {406, "Not Acceptable", "URI not available in preferred format."},
	 {407, "Proxy Authentication Required", "You must authenticate with "
			 "this proxy before proceeding."},
	 {408, "Request Timeout", "Request timed out; try again later."},
	 {409, "Conflict", "Request conflict."},
	 {410, "Gone",
			 "URI no longer exists and has been permanently removed."},
	 {411, "Length Required", "Client must specify Content-Length."},
	 {412, "Precondition Failed", "Precondition in headers is false."},
	 {413, "Request Entity Too Large", "Entity is too large."},
	 {414, "Request-URI Too Long", "URI is too long."},
	 {415, "Unsupported Media Type", "Entity body in unsupported format."},
	 {416, "Requested Range Not Satisfiable",
			 "Cannot satisfy request range."},
	 {417, "Expectation Failed",
			 "Expect condition could not be satisfied."},

	 {500, "Internal Server Error", "Server got itself in trouble"},
	 {501, "Not Implemented",
			 "Server does not support this operation"},
	 {502, "Bad Gateway", "Invalid responses from another server/proxy."},
	 {503, "Service Unavailable",
			 "The server cannot process the request due to a high load"},
	 {504, "Gateway Timeout",
			 "The gateway server did not receive a timely response"},
	 {505, "HTTP Version Not Supported", "Cannot fulfill request."}
};

const char* http_status_line(int status)
{
	int low = 0;
	int high = sizeof(http_status_infos) / sizeof(HttpStatusInfo);
	int mid = (low + high) / 2;

	while(low <= high)
	{
		if(http_status_infos[mid].status == status) 
			return http_status_infos[mid].line;
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

int open_listen_fd(char* ip, int port)
{
	int listen_fd = -1;
	struct sockaddr_in addr;
	int opt = 1;

	listen_fd = socket(AF_INET, SOCK_STREAM, 0);
	if(listen_fd < 0) return -1;

	if(setsockopt(listen_fd, SOL_SOCKET, 
					SO_REUSEADDR, (const void* )&opt, sizeof(int)) < 0)
		return -1;
	
	bzero((char* )&addr, sizeof(struct sockaddr_in));

	addr.sin_family = AF_INET;
	if(ip != NULL)
		addr.sin_addr.s_addr = inet_addr(ip);
	else
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	addr.sin_port = htons((unsigned short) port);

	if(bind(listen_fd, (struct sockaddr* )&addr, sizeof(struct sockaddr_in)) < 0)
		return -1;

	if(listen(listen_fd, 20) < 0)
		return -1;
	
	return listen_fd;
}

int get_token(str_t* str, char** buf, IS_DELIM_FUNC delim_func, const char* delim)
{
	if(buf==NULL || *buf ==NULL) return NULL;

	char* p = *buf;
	char* p_start;

	while(*p!='\0' && IS_DELIM(delim_func, delim, *p)) p++;

	if(*p == '\0') return 0;
	
	p_start = p;
	p++;
	while(*p!='\0' && !IS_DELIM(delim_func, delim, *p)) p++;

	str->data = p_start;
	str->len = p - p_start;
	*p = '\0';
	*buf = p + 1;

	return 1;
}

char* read_file(const char* file_name)
{
	char* buffer = NULL;
	FILE* fp = fopen(file_name, "r");

	if(fp != NULL)
	{
		struct stat st = {0};
		if(stat(file_name, &st) == 0)
		{
			buffer = malloc(st.st_size + 1);
			fread(buffer, st.st_size, 1, fp);
			buffer[st.st_size] = '\0';
		}
		fclose(fp);
	}

	return buffer;
}

int nwrite(int fd, char* buf, size_t len)
{
	assert(fd>=0 && buf!=NULL && len>0);

	int cur = 0;
	int written = 0;

	while (cur < len) 
	{
		do   
		{    
			written = write (fd, buf + cur, len - cur);
		} while (written<=0 && (errno==EINTR || errno==EAGAIN));

		if (written <= 0) 
		{    
			printf("nwrite failed: written=%d, errno: %d\n", written, errno);
			return 0;
		}    

		cur += written;
	}  

	return 1;
}

const char* http_content_type(const char* extension)
{
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

const str_t* http_header_str(array_t* headers, const char* name)
{
	assert(headers!=NULL && name!=NULL);

	str_t hname = {name, strlen(name)};

	int i = 0;
	for(; i<headers->count; i++)
	{
		if(strncasecmp(hname.data, h[i].name.data, name.len) == 0)
			return &h[i].value;
	}

	return NULL;
}

int http_header_int(array_t* headers, const char* name)
{
	const str_t* value = http_header_str(headers, name);

	return atoi(value.data);
}

int http_header_equal(array_t* headers, const char* name, const char* value)
{
	const str_t* header_value = http_header_str(h, name);

	if(header_value != NULL && strncasecmp(header_value.data, value, header_value.len) == 0) 
		return 1;
	else 
		return 0;
}

int buf_create(buf_t* thiz, pool_t* pool, int size)
{
	assert(thiz!=NULL && pool!=NULL && size>0);

	thiz->start = pool_alloc(pool, size);
	if(thiz->start == NULL) return 0;

	thiz->end = thiz->start + size;
	thiz->pos = thiz->start;
	thiz->last = thiz->start;

	return 1;
}

int array_init(array_t* thiz, pool_t* pool, int nalloc)
{
	assert(thiz!=NULL && pool!=NULL && size > 0);

	thiz->elts = (void** ) pool_alloc(pool, nalloc * sizeof(void*));
	if(thiz->elts == NULL) return 0;

	thiz->count = 0;
	thiz->nalloc = nalloc;
	thiz->pool = pool;

	return 1;
}

int array_push(array_t* thiz, void* data)
{
	assert(thiz != NULL);
	if(thiz->count == thiz->nalloc)
	{
		void** new  = pool_alloc(thiz->pool, thiz->nalloc + thiz->nalloc / 2);

		if(new == NULL) return 0;

		memcpy(new, thiz->data, thiz->nalloc * sizeof(void*));

		thiz->elts = new;
		thiz->nalloc = thiz->nalloc + thiz->nalloc / 2;
	}
	
	thiz->elts[thiz->count] = data;	
	thiz->count += 1;

	return 1;
}


