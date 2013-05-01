#ifndef CONF_H
#define CONF_H
#include <regex.h>
#include <sys/types.h>
#include "pool.h"
#include "array.h"
#include "typedef.h"
typedef struct module_s module_t;
typedef struct vhost_conf_s vhost_conf_t;
typedef struct conf_s conf_t;
typedef struct _XmlNode XmlNode;

//FIXME multiple redefinition.
typedef module_t* (*MODULE_CREATE_FUNC)(void* ctx, XmlNode* xml_conf_node, pool_t* pool);//must hook the destroy handler on the pool cleanup. 

typedef enum
{
	MODULE_TYPE_HANDLER = 0,
	MODULE_TYPE_FILTER,
}module_type_e;

typedef struct module_param_s
{
	str_t name;
	array_t* values; //str_t*
}module_param_t;

typedef struct vhost_loc_conf_s 
{
	vhost_conf_t* parent;
	str_t root;
	str_t pattern_str;
	regex_t pattern_regex;
	
	//array_t* handler_params; // module_param_t*
	str_t handler_name;
	module_t* handler;
}vhost_loc_conf_t;

typedef struct vhost_conf_s
{
	conf_t* parent;
	str_t name;
	str_t root;
	//array_t default_pages;

	array_t* locs; //vhost_loc_conf_t*
	//TODO
	vhost_loc_conf_t* default_loc;
}vhost_conf_t;

typedef struct module_so_conf_s
{
	conf_t* parent;
	str_t name;
	str_t author;
	str_t description;
	str_t version;
	module_type_e module_type; /*0 handler, 1 filter.*/

	MODULE_CREATE_FUNC module_create;
	//MODULE_CONF_PARSE_FUNC module_conf_parse;
}module_so_conf_t;

typedef struct conf_s
{
	pool_t* pool;
	int request_pool_size;
	int connection_timeout;
	int client_header_size;
	int large_client_header_size;
	int max_content_len;
	str_t ip;
	int port;
	str_t root;
	str_t default_page;
	int max_threads;
	//array_t default_pages; //str_t

	str_t module_path;
	array_t* module_sos;    //module_so_conf_t*
	array_t* vhosts;        //vhost_conf_t*
}conf_t;

conf_t* conf_parse(const char* config_file, pool_t* pool);

#endif
