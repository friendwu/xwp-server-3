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

typedef struct server_s
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
}server_t;

static void* server_listen_proc(void* ctx)
{
	server_t* thiz = (server_t* )ctx;
	int fd;
	struct sockaddr_in peer_addr;
	socklen_t sock_len = (socklen_t) sizeof(struct sockaddr_in);
	connection_t* c = NULL;

	while(thiz->running)
	{
		fd = accept(thiz->listen_fd, (struct sockaddr* )&peer_addr, &sock_len);
		if(fd < 0) continue;

		pthread_mutex_lock(&thiz->mutex);
		c = thiz->free_connections;
		thiz->free_connections = c->next;
		pthread_mutex_unlock(&thiz->mutex);

		assert(c!=NULL && c->running == 0);

		connection_run(c, fd);
		
		assert(c->running == 0);

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
			if(thiz->connections[i]->running == 0) continue;
			connection_check_timeout(thiz->connections[i]);
		}

		sleep(1);
	}

	return (void* )NULL;
}

server_t* server_create(const char* config_file)
{
	pool_t* pool = pool_create(2 * 1024);

	server_t* thiz = pool_calloc(pool, sizeof(server_t));
	if(thiz == NULL) 
	{
		pool_destroy(pool);
		return NULL;
	}
	thiz->pool = pool;

	signal(SIGPIPE, SIG_IGN);

	conf_t* conf = conf_parse(config_file, pool);
	if(conf == NULL)
	{
		pool_destroy(pool);
		return NULL;
	}
	thiz->conf = conf;

	thiz->connections = (connection_t** )pool_calloc(pool, conf->max_threads * sizeof(connection_t*));
	int i = 0;
	for(i=0; i<conf->max_threads-1; i++)
	{
		thiz->connections[i] = connection_create(pool, conf);
		if(thiz->connections[i] == NULL) 
		{
			pool_destroy(pool);
			return NULL;
		}
		thiz->connections[i]->next = thiz->connections[i + 1];
	}

	thiz->connections[conf->max_threads-1]->next = NULL;
	thiz->connection_nr = conf->max_threads;

	thiz->listen_fd = open_listen_fd(conf->ip, conf->port);
	if(thiz->listen_fd < 0) 
	{
		printf("open listen fd failed.\n");
		pool_destroy(pool);
		return NULL;
	}

	pthread_mutex_init(&thiz->mutex, NULL);
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 256*1024); 
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	thiz->running = 1;
	thiz->listen_tids = pool_alloc(pool, conf->max_threads);
	int k = 0;
	for(; k<conf->max_threads; k++)
	{
		pthread_create(&thiz->listen_tids[k], &attr, server_listen_proc, thiz);
	}
	pthread_create(&thiz->guard_tid, &attr, server_guard_proc, thiz);
	
	pthread_attr_destroy(&attr);

	return thiz;
}

int server_destroy(server_t* thiz)
{
	assert(thiz!=NULL);
	if(thiz->listen_fd >= 0) close(thiz->listen_fd);

	thiz->running = 0;
	/*//TODO hook this in connection_create.
	int k = 0;
	for(k; k<thiz->connection_nr; k++)
	{
		connection_close(thiz->connections[k]);
	}*/

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

