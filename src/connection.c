#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include "connection.h"
#include "module.h"
#include "typedef.h"
#include "buf.h"
#include "array.h"
#include "upstream.h"

static void connection_gen_request(connection_t* thiz)
{
	int ret = 1;

	ret = http_process_request_line(thiz->r, thiz->fd);
	if(ret == 0) return;

	ret = http_process_header_line(thiz->r, thiz->fd, HTTP_PROCESS_PHASE_REQUEST);
	if(ret == 0) return;

	http_process_content_body(thiz->r, thiz->fd, &thiz->r->body_in, 
							  thiz->r->headers_in.content_len);

	return;
}

static size_t connection_response_headers_len(connection_t* thiz)
{
	size_t new_line_len = 2;
	size_t len = 0;
	const str_t* status_line = http_status_line(thiz->r->status);
	
	len += sizeof("HTTP/1.x ") - 1;
	len += status_line->len;
	len += new_line_len; 

	http_header_t** header_elts = (http_header_t**) thiz->r->headers_out.headers->elts;
	int header_count = thiz->r->headers_out.headers->count;
	int i = 0; 
	for(; i<header_count; i++)
	{
		len += header_elts[i]->name.len;
		len += header_elts[i]->value.len;
		len += 4; //strlen(\r\n) + ': ' after name.
	}

	len += new_line_len;

	return len;
}

//success return 1 else return 0.
static int connection_send_response(connection_t* thiz)
{
	int count = 0;
	array_t* headers = thiz->r->headers_out.headers;
	http_content_body_t* body_out = &thiz->r->body_out;

	//TODO the ouput header filter should be segregated into a standalone module.
	//TODO add some crucial headers such as: connection, date, server...
	if(thiz->r->keep_alive)
	{
		http_header_set(headers, HTTP_HEADER_CONNECTION, HTTP_HEADER_KEEPALIVE);
	}
	else
	{
		http_header_set(headers, HTTP_HEADER_CONNECTION, HTTP_HEADER_CLOSE);
	}
	http_header_set(headers, HTTP_HEADER_SERVER, HTTP_HEADER_XWP_VER);

	if(body_out->content_len > 0)
	{
		char clen[64] = {0};
		int count = snprintf(clen, 64, "%d", body_out->content_len);
		str_t clen_str = {clen, count};
		http_header_set(headers, HTTP_HEADER_CONTENT_LEN, &clen_str);
	}

	int num = 0;
	int headers_len = connection_response_headers_len(thiz);
	char* buf = pool_alloc(thiz->request_pool, headers_len);
	const str_t* status_line = http_status_line(thiz->r->status);

	count = snprintf(buf, headers_len-num, "%s %s\r\n", thiz->r->version_str.data, 
										  status_line->data);
	if(count <= 0) return 0;
	num += count;

	int header_count = headers->count;
	int i = 0;
	for(; i<header_count; i++)
	{
		http_header_t** header_elts = (http_header_t**) thiz->r->headers_out.headers->elts;
		count = snprintf(buf+num, headers_len-num, "%s: %s\r\n", 
								  header_elts[i]->name.data, header_elts[i]->value.data);
		if(count <= 0) return 0;
		num += count;
	}

	buf[num++] = '\r';
	buf[num++] = '\n';
	assert(num == headers_len);

	if(!nwrite(thiz->fd, buf, num)) return 0;

	assert((body_out->content_fd>=0 && body_out->content==NULL)
			|| (body_out->content_fd<0 && body_out->content!=NULL));

	if(body_out->content_fd >= 0)
	{
		ssize_t ret = sendfile(thiz->fd, body_out->content_fd, NULL, body_out->content_len);
	
		close(body_out->content_fd);
		body_out->content_fd = -1;

		if(ret < 0) return 0;
	}
	else if(body_out->content != NULL)
	{
		if(!nwrite(thiz->fd, body_out->content, body_out->content_len)) 
			return 0;
	}

	return 1;
}

static void connection_process_request(connection_t* thiz)
{
	vhost_conf_t** vhosts = (vhost_conf_t** )thiz->conf->vhosts->elts;
	size_t vhost_nr = thiz->conf->vhosts->count;
	size_t i = 0;
	for(; i<vhost_nr; i++)
	{
		//TODO host header or url host, 
		//in 1.0 version, is the host header indispensable?
		if(strncasecmp(thiz->r->headers_in.header_host->data, 
					   vhosts[i]->name.data, 
					   vhosts[i]->name.len) == 0)
		{
			break;
		}
	}

	if(i == vhost_nr) 
	{
		printf("cannot find the vhost: %s\n", thiz->r->headers_in.header_host->data);
		thiz->r->status = HTTP_STATUS_BAD_REQUEST;
		return;
	}
		
	vhost_loc_conf_t** locs = (vhost_loc_conf_t** ) vhosts[i]->locs->elts;
	size_t loc_nr = vhosts[i]->locs->count;

	int k = 0;
	for(; k<loc_nr; k++)
	{
		if(regexec(&(locs[k]->pattern_regex), thiz->r->url.path.data, 0, NULL, 0) == 0)
			break;
	}

	//TODO there should be a default handler.
	if(k == loc_nr) 
	{
		printf("cannot find the location: %s\n", thiz->r->url.path.data);
		thiz->r->status = HTTP_STATUS_BAD_REQUEST;
		return;
	}
	
	int ret = HTTP_MODULE_PROCESS_DONE;
	ret = module_process(locs[k]->handler, thiz->r);

	if(ret == HTTP_MODULE_PROCESS_DONE)
	{
		return;
	}
	else if(ret == HTTP_MODULE_PROCESS_UPSTREAM)
	{
		upstream_process(thiz->r->upstream);
	}
	else
	{
		assert(0);
		thiz->r->status = HTTP_STATUS_INTERNAL_SERVER_ERROR;
		return;
	}

	return;
}

static int connection_finalize_request(connection_t* thiz)
{
	pthread_mutex_lock(&(thiz->mutex));
	if(!thiz->timedout)
	{
		thiz->start_time = time(NULL);
		if(thiz->r->upstream != NULL) 
			upstream_abort(thiz->r->upstream);
		thiz->r->upstream = NULL;
	}
	pthread_mutex_unlock(&(thiz->mutex));

	pool_destroy(thiz->request_pool);

	if(thiz->timedout) return 0;

	thiz->request_pool = pool_create(thiz->conf->request_pool_size);
	if(thiz->request_pool == NULL) return 0;

	thiz->r = http_request_create(thiz->request_pool, thiz->conf, thiz->peer_addr);
	if(thiz->r == NULL) 
	{
		pool_destroy(thiz->request_pool);
		thiz->request_pool = NULL;
		return 0;
	}

	return 1;
}

static void connection_special_response(connection_t* thiz)
{
	//TODO maybe need to process by filter.
	str_t* error_page = http_error_page(thiz->r->status, thiz->request_pool);
	http_content_body_t* body_out = &thiz->r->body_out;
	
	assert(body_out->content_len == 0 && body_out->content == NULL);

	if(error_page != NULL)
	{
		body_out->content_len = error_page->len;
		body_out->content = error_page->data;
	}

	if(thiz->r->status >= HTTP_STATUS_BAD_REQUEST)
	{
		thiz->r->keep_alive = 0;
	}

	return;
}

static int connection_reusable(connection_t* thiz, int reusable)
{
	assert(thiz!=NULL);	
	assert(thiz->state == CONNECTION_BEFORE_REUSEABLE);

	pthread_mutex_lock(&thiz->mutex);
	thiz->timedout = 0;
	thiz->start_time = 0;
	if(thiz->fd >= 0)
	{
		close(thiz->fd);
		thiz->fd = -1;
	}

	if(thiz->r!=NULL && thiz->r->upstream!=NULL) 
	{
		upstream_abort(thiz->r->upstream);
		thiz->r->upstream = NULL;
	}
	pthread_mutex_unlock(&thiz->mutex);

	if(thiz->request_pool != NULL) pool_destroy(thiz->request_pool);
	thiz->request_pool = NULL;
	thiz->r = NULL;

	if(!reusable)
	{
		pthread_mutex_destroy(&thiz->mutex);
		thiz->state = CONNECTION_UNREUSEABLE;
	}
	else
	{
		thiz->state = CONNECTION_REUSEABLE;
	}

	return 1;
}

static void connection_close(void* ctx)
{
	connection_t* thiz = (connection_t* ) ctx;
	thiz->state = CONNECTION_BEFORE_REUSEABLE;

	connection_reusable(thiz, 0);
}

connection_t* connection_create(pool_t* pool, const conf_t* conf)
{
	assert(pool!=NULL && conf!=NULL);

	connection_t* thiz = pool_calloc(pool, sizeof(connection_t));
	if(thiz == NULL) return NULL;

	pool_add_cleanup(pool, connection_close, thiz);
	
	pthread_mutex_init(&thiz->mutex, NULL);
	thiz->pool = pool;
	thiz->conf = conf;
	thiz->fd = -1;
	thiz->state = CONNECTION_REUSEABLE;
	//others default set zero or NULL.

	return thiz;
}

int connection_run(connection_t* thiz, int fd, struct sockaddr_in* peer_addr)
{
	assert(thiz!=NULL && fd>=0);
	assert(thiz->state == CONNECTION_REUSEABLE);

	thiz->peer_addr = peer_addr;
	thiz->request_pool = pool_create(thiz->conf->request_pool_size);
	if(thiz->request_pool == NULL) return 0;

	thiz->r = http_request_create(thiz->request_pool, thiz->conf, peer_addr);
	if(thiz->r == NULL)
	{
		pool_destroy(thiz->request_pool);
		thiz->request_pool = NULL;
		return 0;
	}

	thiz->start_time = time(NULL);
	thiz->fd = fd;
	thiz->state = CONNECTION_RUNNING;

	while(!thiz->timedout)
	{
		//only this thread are permitted to change it.
		assert(thiz->state == CONNECTION_RUNNING); 

		connection_gen_request(thiz);
		if(thiz->r->status >= HTTP_STATUS_SPECIAL_RESPONSE)
		{
			goto DONE;
		}

		if(thiz->timedout) break;
		connection_process_request(thiz);
		if(thiz->timedout) break;
DONE:
		if(thiz->r->status == HTTP_STATUS_CLOSE) 
		{
			break;
		}
		else if(thiz->r->status >= HTTP_STATUS_SPECIAL_RESPONSE)
		{
			connection_special_response(thiz);
		}

		if(!connection_send_response(thiz)) break;

		if(thiz->r->keep_alive && !thiz->timedout)
		{
			if(!connection_finalize_request(thiz)) break;

			continue;
		}
		else
		{
			break;
		}
	}
	
	thiz->state = CONNECTION_BEFORE_REUSEABLE;
	connection_reusable(thiz, 1);

	return 1;
}

/*
	this function just set timedout flag and help blocked connection_run thread exit, 
	no other cleanup operation will be done here.
*/
int connection_check_timeout(connection_t* thiz)
{
	assert(thiz != NULL);
	if(thiz->state != CONNECTION_RUNNING) return 1;

	time_t now = time(NULL);
	pthread_mutex_lock(&thiz->mutex);
	if(thiz->state == CONNECTION_RUNNING)
	{
		if(now-thiz->start_time >= thiz->conf->connection_timeout)
		{
			thiz->timedout = 1;
			if(thiz->fd >= 0) 
			{
				//add this can speed up the return of connection_run thread.
				shutdown(thiz->fd, SHUT_RDWR);
				close(thiz->fd);
				while(thiz->state != CONNECTION_BEFORE_REUSEABLE) usleep(3000);
				thiz->fd = -1;
			}
			if(thiz->r->upstream != NULL) 
			{
				upstream_abort(thiz->r->upstream);
				while(thiz->state != CONNECTION_BEFORE_REUSEABLE) usleep(3000);
				thiz->r->upstream = NULL;
			}
		}
	}
	pthread_mutex_unlock(&thiz->mutex);

	return 1;
}

