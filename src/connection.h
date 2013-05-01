#ifndef CONNECTION_H
#define CONNECTION_H
#include <pthread.h>
#include "utils.h"
#include "conf.h"
#include "pool.h"
#include "http.h"
struct sockaddr_in;

typedef enum
{
	CONNECTION_REUSEABLE = 0,
	CONNECTION_RUNNING,
	CONNECTION_BEFORE_REUSEABLE,
	CONNECTION_UNREUSEABLE,
}connection_state_e;

typedef struct connection_s
{
	struct connection_s* next;

	pool_t* pool;
	const conf_t* conf;
	connection_state_e state;
	int timedout;
	int fd;
	time_t start_time;
	struct sockaddr_in* peer_addr;
	pthread_mutex_t mutex;

	pool_t* request_pool;
	http_request_t* r;
}connection_t;

connection_t* connection_create(pool_t* pool, const conf_t* conf);

/*return 1 on success, return 0 on fail.*/
int connection_run(connection_t* thiz, int fd, struct sockaddr_in* peer_addr);
int connection_check_timeout(connection_t* thiz);

#endif
