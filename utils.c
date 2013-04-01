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

#define IS_DELIM(delim_func, s, c) ((delim_func!=NULL && delim_func(c)) || (s!=NULL && strchr(s, c)!=NULL))

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
	if(buf==NULL || *buf==NULL) return 0;

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


