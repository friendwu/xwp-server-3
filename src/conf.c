#include <dlfcn.h>
#include <string.h>
#include <assert.h>
#include "log.h"
#include "conf.h"
#include "module.h"
#include "utils.h"
#include "xml_tree.h"
#include "xml_parser.h"
#include "xml_builder_tree.h"

#define DEFAULT_PORT 80
#define DEFAULT_MAX_THREADS 50
#define DEFAULT_CONNECTION_TIMEOUT 30
#define DEFAULT_REQUEST_POOL_SIZE 1024 * 8   
#define DEFAULT_CLIENT_HEADER_SIZE 1024 * 2
#define DEFAULT_LARGE_CLIENT_HEADER_SIZE 1024 * 4
#define DEFAULT_CONTENT_BODY_BUF_SIZE 4 * 1024
#define DEFAULT_MAX_CONTENT_LEN 1024 * 1024

static void conf_parse_loadmodule(conf_t* thiz, XmlNode* root)
{
	pool_t* pool = thiz->pool;
	void* handler = NULL;
	MODULE_GET_INFO_FUNC module_get_info = NULL;

	int i = 0;
	for(;;)
	{
		char* so_file = xml_tree_str(root, "module", i++);
		if(so_file == NULL) break;

		handler = dlopen(so_file, RTLD_NOW);
		if(handler == NULL) 
		{
			log_error("dlopen failed: %s, so file: %s", dlerror(), so_file);
			exit(-1);
		}

		module_get_info = (MODULE_GET_INFO_FUNC)dlsym(handler, "module_get_info");
		if(module_get_info == NULL)
		{
			log_error("dlsym failed: %s", dlerror());
			exit(-1);
		}

		module_so_conf_t* so = pool_calloc(pool, sizeof(module_so_conf_t));
		if(so == NULL) 
		{
			log_error("module_so_conf_t memory allocate failed.");
			exit(-1);
		}

		so->parent = thiz;

		module_get_info(so, pool);

		so->dl_handler = handler;
		array_push(thiz->module_sos, so);
	}

	if(i == 0)
	{
		log_error("at least one module needed.");
		exit(-1);
	}

	return;
}

/*
static void conf_parse_defaultpages(conf_t* thiz, XmlNode* root, pool_t* pool)
{
	char* page = NULL; 
	int i = 0;
	for(;;)
	{
		page = pool_strdup(pool, xml_tree_str(root, "page", i++));
		if(page == NULL) break;

		array_push(thiz->default_pages, page);
	}
}
*/

static void conf_parse_location(conf_t* thiz, vhost_conf_t* vhost, XmlNode* root)
{
	pool_t* pool = thiz->pool;
	vhost_loc_conf_t* loc = pool_calloc(pool, sizeof(vhost_loc_conf_t));
	if(loc == NULL)
	{
		log_error("vhost_loc_conf_t memory allocate failed.");
		exit(-1);
	}

	pool_strdup2(pool, &loc->root, xml_tree_str_first(root, "root"));
	if(loc->root.data == NULL) loc->root = vhost->root;

	pool_strdup2(pool, &loc->pattern_str, xml_tree_str_first(root, "pattern"));
	if(loc->pattern_str.data == NULL)
	{
		log_error("pattern str needed.");
		exit(-1);
	}
	regcomp(&loc->pattern_regex, loc->pattern_str.data, 0);

	XmlNode* handler_node = xml_tree_find_first(root, "handler");
	if(handler_node == NULL)
	{
		log_error("at least one handler needed.");
		exit(-1);
	}
	XmlNode* handler_conf_node = xml_tree_find_first(handler_node, "handler_conf"); 
	pool_strdup2(pool, &loc->handler_name, xml_tree_str_first(handler_node, "name"));
	if(loc->handler_name.data == NULL)
	{
		log_error("handler name needed.");
		exit(-1);
	}

	int m = 0;
	for(;m<thiz->module_sos->count; m++)
	{
		module_so_conf_t* so = (module_so_conf_t* )(thiz->module_sos->elts[m]);
		if(strcmp(loc->handler_name.data, so->name.data) == 0)
		{
			loc->handler = so->module_create(loc, handler_conf_node, pool);
			break;
		}
	}

	if(loc->handler == NULL)
	{
		log_error("handler %s not found.", loc->handler_name.data);
		exit(-1);
	}

	loc->parent = vhost;
	array_push(vhost->locs, loc);
}

static void conf_parse_vhost(conf_t* thiz, XmlNode* root)
{
	pool_t* pool = thiz->pool;

	vhost_conf_t* vhost = pool_calloc(pool, sizeof(vhost_conf_t));
	if(vhost == NULL)
	{
		log_error("vhost_conf_t memory allocate failed.");
		exit(-1);
	}

	pool_strdup2(pool, &vhost->name, xml_tree_str_first(root, "name"));
	pool_strdup2(pool, &vhost->root, xml_tree_str_first(root, "root"));
	if(vhost->name.data == NULL)
	{
		log_error("invalid vhost: vhost name null");
		exit(-1);
	}
	if(vhost->root.data == NULL) vhost->root = thiz->root;

	vhost->locs = array_create(pool, 10);
	if(vhost->locs == NULL)
	{
		log_error("vhost_loc_conf array memory allocate failed.");
		exit(-1);
	}

	int i = 0;
	for(;;)
	{
		XmlNode* node = xml_tree_find(root, "location", i++);

		if(node == NULL) break;

		conf_parse_location(thiz, vhost, node);
	}
	if(i == 0)
	{
		log_error("at least one vhost location needed.");
		exit(-1);
	}

	/*vhost->default_loc = (vhost_loc_conf_t* )(vhost->locs->elts[0]);
	log_info("vhost %s's default location has set to: %s", 
				vhost->name, vhost->default_loc->pattern_str);*/


	vhost->parent = thiz;
	array_push(thiz->vhosts, vhost);
}

static void dump_conf(conf_t* thiz)
{
	log_debug("dump config -------------------");
	log_debug("request_pool_size==> %d", thiz->request_pool_size);
	log_debug("connection_timeout==> %d", thiz->connection_timeout);
	log_debug("client_header_size==> %d", thiz->client_header_size);
	log_debug("large_client_header_size==> %d", thiz->large_client_header_size);
	log_debug("content_body_buf_size==> %d", thiz->content_body_buf_size);
	log_debug("max_content_len==> %d", thiz->max_content_len);
	log_debug("ip==> %s", thiz->ip.data);
	log_debug("port==> %d", thiz->port);
	log_debug("root==> %s", thiz->root.data);
	log_debug("default_page==> %s", thiz->default_page.data);
	log_debug("max_threads==> %d", thiz->max_threads);

	int m = 0;
	for(; m<thiz->module_sos->count; m++)
	{
		module_so_conf_t* so = (module_so_conf_t* )(thiz->module_sos->elts[m]);
		assert(so->parent == thiz);

		log_debug("module_so_conf %d, name==> %s", m, so->name.data);
		log_debug("module_so_conf %d, author==> %s", m, so->author.data);
		log_debug("module_so_conf %d, description==> %s", m, so->description.data);
		log_debug("module_so_conf %d, version==> %s", m, so->version.data);
		log_debug("module_so_conf %d, module_type==> %d", m, so->module_type); /*0 handler, 1 filter.*/
	}

	int n = 0;
	for(; n<thiz->vhosts->count; n++)
	{
		vhost_conf_t* vhost = (vhost_conf_t* )(thiz->vhosts->elts[n]);
		assert(vhost->parent == thiz);
		
		log_debug("vhost_conf %d, name==> %s", n, vhost->name.data);
		log_debug("vhost_conf %d, root==> %s", n, vhost->root.data);

		int k = 0;
		for(; k<vhost->locs->count; k++)
		{
			vhost_loc_conf_t* loc = (vhost_loc_conf_t* )(vhost->locs->elts[k]);
			assert(loc->parent == vhost);
			log_debug("vhost_loc_conf %d, root==> %s", k, loc->root.data);
			log_debug("vhost_loc_conf %d, pattern==> %s", k, loc->pattern_str.data);
			log_debug("vhost_loc_conf %d, handler==> %s", k, loc->handler_name.data);
		}
	}

	log_debug("dump config end-------------------");

	return;
}


static void conf_destroy(void* data)
{
	conf_t* thiz = (conf_t* ) data;

	int i;
	for(i=0; i<thiz->vhosts->count; i++)
	{
		vhost_conf_t* vhost = (vhost_conf_t* ) (thiz->vhosts->elts[i]);
		for(i=0; i<vhost->locs->count; i++)
		{
			vhost_loc_conf_t* loc = (vhost_loc_conf_t* ) (vhost->locs->elts[i]);

			regfree(&loc->pattern_regex);
		}
	}
}

conf_t* conf_parse(const char* conf_file, pool_t* pool)
{
	//TODO hook the destroy handler to the pool.
	assert(conf_file != NULL && pool != NULL);

	conf_t* thiz = NULL;
	XmlNode* tree = NULL;
	XmlBuilder* builder = xml_builder_tree_create();
	XmlParser* parser = xml_parser_create();
	char* buffer = read_file(conf_file);

	if(buffer==NULL 
			|| builder==NULL 
			|| parser==NULL) 
	{
		log_error("xml parser init failed.");
		exit(-1);
	}

	xml_parser_set_builder(parser, builder);
	xml_parser_parse(parser, buffer);
	tree = xml_builder_get_tree(builder);
	if(tree == NULL)
	{
		log_error("get xml tree nodes failed.");
		exit(-1);
	}

	thiz = pool_calloc(pool, sizeof(conf_t));
	if(thiz == NULL) 
	{
		log_error("conf_t memory allocate faild.");
		exit(-1);
	}
	thiz->pool = pool;

	if((thiz->module_sos = array_create(pool, 10)) == NULL)
	{
		log_error("array create failed.");
		exit(-1);
	}
	if((thiz->vhosts = array_create(pool, 10)) == NULL) 
	{
		log_error("array create failed.");
		exit(-1);
	}
	/*
	if((thiz->default_pages = array_create(pool, 10)) == NULL) 
	{
		log_error("array create failed.");
		exit(-1);
	}*/

	XmlNode* root = tree;

	thiz->max_threads = xml_tree_int_first(root, "max_threads", DEFAULT_MAX_THREADS);
	thiz->request_pool_size = xml_tree_int_first(root, "request_pool_size", DEFAULT_REQUEST_POOL_SIZE);
	thiz->connection_timeout = xml_tree_int_first(root, "connection_timeout", DEFAULT_CONNECTION_TIMEOUT);
	thiz->client_header_size = xml_tree_int_first(root, "client_header_size", DEFAULT_CLIENT_HEADER_SIZE);
	thiz->large_client_header_size = xml_tree_int_first(root, "large_client_header_size", DEFAULT_LARGE_CLIENT_HEADER_SIZE);
	thiz->content_body_buf_size = xml_tree_int_first(root, "content_body_buf_size", DEFAULT_CONTENT_BODY_BUF_SIZE);
	thiz->max_content_len = xml_tree_int_first(root, "max_content_len", DEFAULT_MAX_CONTENT_LEN);
	thiz->port = xml_tree_int_first(root, "port", DEFAULT_PORT);
	pool_strdup2(pool, &thiz->ip, xml_tree_str_first(root, "ip"));
	pool_strdup2(pool, &thiz->root, xml_tree_str_first(root, "root"));
	if(thiz->root.data == NULL)
	{
		log_error("root directory needed.");
		exit(-1);
	}

	to_string(thiz->default_page, "index.html");

	conf_parse_loadmodule(thiz, xml_tree_find_first(root, "load_module"));
	
	int i = 0;
	for(;;)
	{
		XmlNode* node = xml_tree_find(root, "vhost", i++);
		if(node == NULL) break;
		conf_parse_vhost(thiz, node);
	}
	if(i == 0)
	{
		log_error("at least one vhost needed.");
		exit(-1);
	}

	thiz->default_vhost = (vhost_conf_t* )(thiz->vhosts->elts[0]);
	log_info("default vhost has set to: %s", thiz->default_vhost->name.data);

	DESTROY_MEM(xml_builder_destroy, builder);
	DESTROY_MEM(xml_parser_destroy, parser);
	DESTROY_MEM(xml_node_destroy, tree);
	SAFE_FREE(buffer);

	dump_conf(thiz);

	pool_add_cleanup(pool, conf_destroy, thiz);

	return thiz;
}

/*
static module_so_conf_t* load_module(conf_t* thiz, char* so_file)
{
	void* handler = NULL;
	MODULE_GET_INFO_FUNC module_get_info = NULL;
	pool_t* pool = thiz->pool;

	handler = dlopen(so_file, RTLD_NOW);
	if(handler == NULL) log_error("%s", dlerror());
	assert(handler != NULL);

	module_get_info = (MODULE_GET_INFO_FUNC)dlsym(handler, "module_get_info");
	assert(module_get_info != NULL);
	module_so_conf_t* so = pool_calloc(pool, sizeof(module_so_conf_t));
	assert(so != NULL);
	so->parent = thiz;

	module_get_info(so, pool);
	array_push(thiz->module_sos, so);

	return so;
}

conf_t* conf_parse(const char* conf_file, pool_t* pool)
{
	//TODO hook the destroy handler to the pool.
	conf_t* thiz = pool_calloc(pool, sizeof(conf_t));
	if(thiz == NULL) return NULL;
	thiz->pool = pool;

	thiz->max_threads = 100;
	thiz->request_pool_size = 1024 * 8;
	thiz->connection_timeout = 30;
	thiz->client_header_size = 1024 * 2;
	thiz->large_client_header_size = 1024 * 4;
	thiz->content_body_buf_size = 4 * 1024;
	//TODO maybe this field should apply just to the client body.
	thiz->max_content_len = 1024 * 1024;
	to_string(thiz->ip, "127.0.0.1");
	thiz->port = 9001;
	to_string(thiz->root, "/home/wpeng/uone/mycode/refactor_server");
	to_string(thiz->default_page, "index.html");

	thiz->module_sos = array_create(pool, 10);
	thiz->vhosts = array_create(pool, 10);
	to_string(thiz->module_path, "./modules");

	//loadmodule
	module_so_conf_t* so1 = load_module(thiz, "/usr/local/xwp/modules/libmodule_default.so");
	module_so_conf_t* so2 = load_module(thiz, "/usr/local/xwp/modules/libmodule_uwsgi.so");

	//vhost
	vhost_conf_t* vhost = pool_calloc(pool, sizeof(vhost_conf_t));
	vhost->parent = thiz;
	array_push(thiz->vhosts, vhost);
	vhost->locs = array_create(pool, 10);
	to_string(vhost->name, "localhost");
	vhost->root = thiz->root;

	//static location
	vhost_loc_conf_t* loc = pool_calloc(pool, sizeof(vhost_loc_conf_t));
	assert(loc != NULL);
	loc->parent = vhost;

	to_string(loc->root, "/home/wpeng/uone/mycode/xwp2");
	to_string(loc->pattern_str, "/static/.*");
	//TODO regfree in destroy hook.
	if(regcomp(&loc->pattern_regex, loc->pattern_str.data, 0) != 0)
	{
		log_error("regex compile failed.");
		exit(-1);
	}
	to_string(loc->handler_name, "default");
	loc->handler = so1->module_create(loc, NULL, pool);
	array_push(vhost->locs, loc);

	//uwsgi location
	loc = pool_calloc(pool, sizeof(vhost_loc_conf_t));
	assert(loc != NULL);
	loc->parent = vhost;
	loc->root = thiz->root;
	to_string(loc->pattern_str, "/.*");
	//TODO regfree in destroy hook.
	if(regcomp(&loc->pattern_regex, loc->pattern_str.data, 0) != 0)
	{
		log_error("regex compile failed.");
		exit(-1);
	}

	to_string(loc->handler_name, "uwsgi");
	loc->handler = so2->module_create(loc, NULL, pool);
	array_push(vhost->locs, loc);

	return thiz;
}
*/

