#ifndef CONF_H
#define CONF_H
#include <regex.h>
#include <sys/types.h>
#include "pool.h"
#include "array.h"
#include "typedef.h"
#include "module.h"

typedef enum
{
	MODULE_TYPE_HANDLER = 0,
	MODULE_TYPE_FILTER,
}module_type_e;

struct vhost_loc_conf_s
{
	vhost_conf_t* parent;
	str_t root;
	str_t pattern_str;
	regex_t pattern_regex;

	str_t handler_name;
	module_t* handler;
};

struct vhost_conf_s
{
	conf_t* parent;
	str_t name;
	str_t root;
	//array_t default_pages;

	array_t* locs; //vhost_loc_conf_t*
	//vhost_loc_conf_t* default_loc;
};

struct module_so_conf_s
{
	conf_t* parent;
	str_t name;
	str_t author;
	str_t description;
	str_t version;
	module_type_e module_type; /*0 handler, 1 filter.*/
	void* dl_handler;

	MODULE_CREATE_FUNC module_create;
};

struct conf_s
{
	pool_t* pool;
	int request_pool_size;
	int connection_timeout;
	int client_header_size;
	int large_client_header_size;
	int content_body_buf_size;
	int max_content_len;
	str_t ip;
	int port;
	str_t root;
	str_t default_page;
	int max_threads;
	int worker_num;
	//array_t default_pages; //str_t

	array_t* module_sos;    //module_so_conf_t*
	array_t* vhosts;        //vhost_conf_t*
	vhost_conf_t* default_vhost;
};

conf_t* conf_parse(const char* config_file, pool_t* pool);

#endif
