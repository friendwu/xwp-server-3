#ifndef CONF_H
#define CONF_H
#define MODULE_TYPE_HANDLER 0
#define MODULE_TYPE_FILTER 1
typedef enum
{
	MODULE_TYPE_HANDLER = 0,
	MODULE_TYPE_FILTER,
}module_type_e;

typedef struct module_param_s
{
	char* name;
	array_t values;
}module_param_t;

typedef struct vhost_loc_conf_s 
{
	vhost_conf_t* parent;
	char* root;
	char* pattern_str;
	regex_t pattern_regex;
	
	array_t handler_params; // module_param_t*
	char* handler_name;
	module_t* handler;
}vhost_loc_conf_t;

typedef struct vhost_conf_s
{
	char* name;
	char* root;
	array_t default_pages;

	array_t locs; //vhost_loc_conf_t*
	vhost_loc_conf_t* default_loc;
}vhost_conf_t;

typedef struct module_so_conf_s
{
	char* name;
	char* author;
	char* description;
	char* version;
	module_type_e module_type; /*0 handler, 1 filter.*/

	MODULE_CREATE_FUNC module_create;
}module_so_conf_t;

typedef struct conf_s
{
	pool_t* pool;
	//TODO
	int request_pool_size;
	int connection_timeout;
	int client_header_size;
	int large_client_header_size;
	int max_content_len;

	char* ip;
	int port;
	char* root;
	int max_threads;
	int connection_timeout;
	array_t default_pages; //char*

	char* module_path;
	array_t module_sos;    //module_so_conf_t*
	array_t vhosts;        //vhost_conf_t*
}conf_t;

conf_t* config_parse(const char* config_file, poo_t* pool);

#endif
