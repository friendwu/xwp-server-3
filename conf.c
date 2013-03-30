#include "conf.h"
#include "typedef.h"
#include "pool.h"
#include <dlfcn.h>
#define DEFAULT_PORT 80
#define DEFAULT_MAX_THREADS 50
#define DEFAULT_CONNECTION_TIMEOUT 30
#define DEFAULT_ROOT "."
/*
static void config_parse_loadmodule(conf_t* thiz, XmlNode* root)
{
	pool_t* pool = thiz->pool;
	void* handler = NULL;
	char path_buf[512] = {0};
	MODULE_GET_INFO module_get_info = NULL;
	
	thiz->module_path = pool_strdup(pool, xml_tree_str(root, "path", 0));
	assert(thiz->module_path != NULL);
	int i = 0;
	for(;;)
	{
		char* so_file = xml_tree_str(root, "module", i++);
		if(so_file == NULL) break;

		snprintf(path_buf, 512, "%s/%s", thiz->module_path, so_file);
		handler = dlopen(path_buf, RTLD_NOW);
		assert(handler != NULL);

		module_get_info = (MODULE_GET_INFO_FUNC)(handler, "module_get_info");
		assert(module_get_info != NULL);
		module_so_conf_t* so = pool_calloc(thiz, pool, sizeof(module_so_conf_t));

		assert(so != NULL);
		module_get_info(so, pool);
		array_push(&thiz->module_sos, so);

		handler = NULL;
	}
}

static void config_parse_defaultpages(conf_t* thiz, XmlNode* root, pool_t* pool)
{
	char* page = NULL; 
	int i = 0;
	for(;;)
	{
		page = pool_strdup(pool, xml_tree_str(root, "page", i++));
		if(page == NULL) break;

		array_push(&thiz->default_pages, page);
	}
}

static void config_parse_location(conf_t* thiz, XmlNode* root, vhost_conf_t* vhost)
{
	pool_t* pool = thiz->pool;
	vhost_loc_conf_t* loc = pool_calloc(pool, sizeof(vhost_loc_conf_t));
	assert(loc != NULL);
	
	loc->root = pool_strdup(pool, xml_tree_str(root, "root"), 0);
	loc->pattern = pool_strdup(pool, xml_tree_str(root, "pattern"), 0);
	regcomp(&loc->pattern_reg, loc->pattern, 0);

	XmlNode* hnode = xml_tree_find(root, "handle", 0);
	loc->handler_name = pool_strdup(pool, xml_tree_str(hnode, "name", 0));
	int i = 0;
	for(;;)
	{
		XmlNode* pnode = xml_tree_find(hnode, "param"; i++);
		if(pnode == NULL) break; 

		module_param_t* param = pool_calloc(pool, sizeof(module_param_t));
		
		param->name = pool_strdup(pool, xml_tree_str(pnode, "name", 0));
		int k = 0;
		for(;;)
		{
			char* value = pool_strdup(pool, xml_tree_str(pnode, "value", k++));
			if(value == NULL) break;

			array_push(&param->values, value);
		}

		array_push(&loc->handler_params, param);
	}

	int m = 0;
	for(;m<thiz->module_sos.count; m++)
	{
		module_so_conf_t* so = (module_so_conf_t* )(thiz->module_sos.elts[m]);
		if(strcmp(loc->handler_name, so->name) == 0)
		{
			loc->handler = so->module_create(&loc->handle_params);
		}
	}

	if(loc->handler == NULL)
	{
		printf("handler %s not found.\n", loc->handler_name);
		return;
	}
	
	array_push(&vhost->locs, loc);
}

static void config_parse_vhost(conf_t* thiz, XmlNode* root)
{
	pool_t* pool = thiz->pool;

	vhost_conf_t* vhost = pool_calloc(pool, sizeof(vhost_conf_t));
	assert(vhost != NULL); 

	vhost->name = pool_strdup(pool, xml_tree_str(root, "name", 0));
	vhost->root = pool_strdup(pool, xml_tree_str(root, "root", 0));
	assert(vhost->name != NULL);

	int i = 0;
	for(;;)
	{
		XmlNode* node = xml_tree_find(root, "location", i++);

		if(node == NULL) break;

		config_parse_location(thiz, vhost, node);
	}

	array_push(&thiz->vhosts, vhost);
}

conf_t* config_parse(const char* config_file, pool_t* pool)
{
	//TODO hook the destroy handler to the pool.
	assert(config_file != NULL && pool != NULL);

	conf_t* thiz = NULL;
	XmlNode* tree = NULL;
	XmlBuilder* builder = xml_builder_tree_create();
	XmlParser* parser = xml_parser_create();
	char* buffer = read_file(config_file);
	
	if(buffer==NULL 
		|| builder==NULL 
		|| parser==NULL) goto DONE;
	
	xml_parser_set_builder(parser, builder);
	xml_parser_parse(parser, buffer);
	tree = xml_builder_get_tree(builder);
	if(tree == NULL) goto DONE;

	thiz = pool_calloc(pool, sizeof(conf_t));
	if(thiz == NULL) goto DONE;
	if(!array_init(&thiz->module_sos, pool, 10)) goto DONE;
	if(!array_init(&thiz->vhosts, pool, 10)) goto DONE;
	if(!array_init(&thiz->default_pages, pool, 10)) goto DONE;
	
	XmlNode* node = tree;

	thiz->ip = pool_strdup(pool, xml_tree_str(root, "ip", 0, NULL));
	thiz->port = xml_tree_int(root, "port", 0, DEFAULT_PORT);
	thiz->max_threads = xml_tree_int(root, "max_threads", 0, DEFAULT_MAX_THREADS);
	thiz->connection_timeout = xml_tree_int(root, "connection_timeout", 0, DEFAULT_CONNECTION_TIMEOUT);
	thiz->root = pool_strdup(pool, xml_tree_str(root, "root", 0, NULL));

	node = xml_tree_find(root, "load_module", 0);
	if(node == NULL) goto DONE;
	{
		printf("cannot find load module conf\n");
		exit(-1);
	}
	config_parse_loadmodule(thiz, node);
	
	int i = 0;
	for(;;)
	{
		node = xml_tree_find(root, "vhost", i++);
		if(node == NULL) break;
		config_parse_vhost(thiz, node);
	}

	if(!thiz->ip) NULL;
	if(thiz->max_threads <= 0)	
	{
		thiz->max_threads = DEFAULT_MAX_THREADS;
		printf("max_threads set to default %d\n", DEFAULT_MAX_THREADS);
	}

	if(!thiz->root) 
	{
		thiz->root = pool_strdup(pool, DEFAULT_ROOT);
		printf("root set to default %s\n", DEFAULT_ROOT);
	}

	if(thiz->connection_timeout <= 0) 
	{
		thiz->connection_timeout = DEFAULT_CONNECTION_TIMEOUT;
		printf("connection timeout set to default %d\n", DEFAULT_CONNECTION_TIMEOUT);
	}

DONE:
	DESTROY_MEM(xml_builder_destroy, builder);
	DESTROY_MEM(xml_parser_destroy, parser);
	DESTROY_MEM(xml_node_destroy, tree);
	SAFE_FREE(buffer);

	return thiz;
}
*/

conf_t* conf_parse(const char* config_file, pool_t* pool)
{
	//TODO hook the destroy handler to the pool.
	conf_t* thiz = pool_calloc(pool, sizeof(conf_t));
	
	thiz->max_threads = 1;
	thiz->request_pool_size = 1024 * 8;
	thiz->connection_timeout = 30;
	thiz->client_header_size = 1024 * 2;
	thiz->large_client_header_size = 1024 * 4;
	thiz->max_content_len = 16 * 1024;
	thiz->ip = NULL;
	thiz->port = 9001;
	thiz->root = pool_strdup(pool, ".");

	array_init(&thiz->module_sos, pool, 10);
	array_init(&thiz->vhosts, pool, 10);
	//if(!array_init(&thiz->default_pages, pool, 10)) exit(-1);

	//loadmodule
	void* handler = NULL;
	MODULE_GET_INFO module_get_info = NULL;
	thiz->module_path = pool_strdup(pool, "./modules");
	char* so_file = "./modules/module_default.so";

	handler = dlopen(so_file, RTLD_NOW);
	assert(handler != NULL);

	module_get_info = (MODULE_GET_INFO_FUNC)(handler, "module_get_info");
	assert(module_get_info != NULL);
	module_so_conf_t* so = pool_calloc(thiz, pool, sizeof(module_so_conf_t));
	assert(so != NULL);

	module_get_info(so, pool);
	array_push(&thiz->module_sos, so);

	//vhost
	vhost_conf_t* vhost = pool_calloc(pool, sizeof(vhost_conf_t));
	array_push(&thiz->vhosts, vhost);
	array_init(&vhost->locs, pool, 10);
	vhost->name = pool_strdup(pool, "wpeng.me");
	vhost->root = pool_strdup(pool, ".");

	vhost_loc_conf_t* loc = pool_calloc(pool, sizeof(vhost_loc_conf_t));
	assert(loc != NULL);
	loc->parent = vhost;
	loc->root = pool_strdup(pool, "./static");
	loc->pattern_str = pool_strdup(pool, "/.*");
	//TODO regfree in destroy hook.
	regcomp(&loc->pattern_reg, loc->pattern, 0);
	loc->handler_name = pool_strdup(pool, "default");
	loc->handler = so->module_create(&loc->handle_params, pool);
	array_push(&vhost->locs, loc);

	return thiz;
}

