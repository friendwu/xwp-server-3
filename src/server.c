#include <pthread.h>
#include <signal.h>
#include <time.h>
#include <strings.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "server.h"
#include "utils.h"
#include "pool.h"
#include "connection.h"
#include "utils.h"
#include "conf.h"
#include "log.h"
extern int s_listen_fd;

struct server_s
{
	pool_t* pool;
	conf_t* conf;
	connection_t** connections;
	size_t connection_nr;
	connection_t* free_connections;
	pthread_mutex_t mutex;
	pthread_t* listen_tids;
	pthread_t guard_tid;
	int listen_fd;
	int running;
};

static void* server_listen_proc(void* ctx)
{
	server_t* thiz = (server_t* )ctx;
	int fd = 0;
	struct sockaddr_in peer_addr;
	socklen_t sock_len = (socklen_t) sizeof(struct sockaddr_in);
	connection_t* c = NULL;

	while(thiz->running)
	{
		fd = accept(thiz->listen_fd, (struct sockaddr* )&peer_addr, &sock_len);
		if(fd < 0) continue;

		pthread_mutex_lock(&thiz->mutex);
		assert(thiz->free_connections != NULL);
		c = thiz->free_connections;
		thiz->free_connections = c->next;
		pthread_mutex_unlock(&thiz->mutex);

		assert(c!=NULL && c->state == CONNECTION_REUSEABLE);

		connection_run(c, fd, &peer_addr);

		assert(c->state == CONNECTION_REUSEABLE);

		pthread_mutex_lock(&thiz->mutex);
		c->next = thiz->free_connections;
		thiz->free_connections = c;
		pthread_mutex_unlock(&thiz->mutex);
	}

	return (void* )NULL;
}

static void* server_guard_proc(void* ctx)
{
	server_t* thiz = (server_t* )ctx;

	while(thiz->running)
	{
		int i = 0;
		for(; i<thiz->connection_nr; i++)
		{
			connection_check_timeout(thiz->connections[i]);
		}

		sleep(1);
	}

	return (void* )NULL;
}

server_t* server_create(conf_t* conf)
{
    pool_t* pool = conf->pool;
	server_t* thiz = pool_calloc(pool, sizeof(server_t));
	if(thiz == NULL) return NULL;

	thiz->pool = pool;
	thiz->conf = conf;
	thiz->listen_fd = s_listen_fd;

	thiz->connections = (connection_t** )pool_calloc(pool, conf->max_threads * sizeof(connection_t*));
	int i = 0;
	for(; i<conf->max_threads; i++)
	{
		thiz->connections[i] = connection_create(pool, conf);
		if(thiz->connections[i] == NULL)
		{
			pool_destroy(pool);
			return NULL;
		}
		if(i > 0)
		{
			thiz->connections[i-1]->next = thiz->connections[i];
		}
	}

	thiz->connection_nr = conf->max_threads;
	thiz->free_connections = thiz->connections[0];

	pthread_mutex_init(&thiz->mutex, NULL);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 256*1024);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	thiz->running = 1;
	thiz->listen_tids = pool_alloc(pool, conf->max_threads * sizeof(pthread_t* ));

	int k = 0;
	for(; k<conf->max_threads; k++)
	{
		pthread_create(&thiz->listen_tids[k], &attr, server_listen_proc, thiz);
	}

#ifndef GUARD_DISABLE
	pthread_create(&thiz->guard_tid, &attr, server_guard_proc, thiz);
#endif
	pthread_attr_destroy(&attr);
	log_info("listen on port %d", conf->port);

	return thiz;
}

int server_destroy(server_t* thiz)
{
	assert(thiz!=NULL);
	if(thiz->listen_fd >= 0) close(thiz->listen_fd);

	thiz->running = 0;

	pthread_join(thiz->guard_tid, NULL);
	int i = 0;
	for(; i<thiz->conf->max_threads; i++)
	{
		pthread_join(thiz->listen_tids[i], NULL);
	}

	pthread_mutex_destroy(&thiz->mutex);

	pool_destroy(thiz->pool);

	return 1;
}

