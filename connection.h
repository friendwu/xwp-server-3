#ifndef CONNECTION_H
#define CONNECTION_H
#include "typedef.h"

typedef struct server_s server_t;
typedef struct connection_s
{
	connection_t* next;

	pool_t* pool;
	conf_t* conf;
	int running;
	int timedout;
	int fd;
	time_t start_time;
	pthread_mutex_t mutex;

	http_request_t r;
}connection_t;

connection_t* connection_create(pool_t* pool, conf_t* conf);

/*return 1 on success, return 0 on fail.*/
int connection_run(connection_t* thiz, int fd);
int connection_check_timeout(connection_t* thiz);

#endif
