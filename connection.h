#ifndef CONNECTION_H
#define CONNECTION_H
#include "typedef.h"

typedef struct server_s server_t;
typedef struct connection_s
{
	connection_t* next;

	conf_t* conf;
	int inited;
	int running;
	int timedout;
	int fd;
	time_t start_time;
	pthread_mutex_t mutex;

	http_request_t r;
}connection_t;

/*return 1 on success, return 0 on fail.*/
int connection_init(connection_t* thiz, conf_t* conf);
int connection_run(connection_t* thiz, int fd);
int connection_check_timeout(connection_t* thiz);
int connection_close(connection_t* thiz);

#endif
