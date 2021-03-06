#ifndef TYPEDEF_H
#define TYPEDEF_H
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>

typedef enum _Ret
{
	RET_OK,
	RET_OOM,
	RET_STOP,
	RET_INVALID_PARAMS,
	RET_FAIL
}Ret;

#ifdef __cplusplus
#define DECLS_BEGIN extern "C" {
#define DECLS_END   }
#else
#define DECLS_BEGIN
#define DECLS_END
#endif/*__cplusplus*/


#define DECL_PRIV(thiz, val, type) type val = thiz != NULL ? (type) thiz->priv : NULL

#define DESTROY_MEM(func, p) if((p)) \
	{func(p);} \

#define SAFE_FREE(p) if((p) != NULL) {free((p)); (p) = NULL;}

#define return_if_fail(p) if(!(p)) {return;}
	/*{fprintf(stderr, "%s:%d Warning: "#p" failed.\n", \
		__func__, __LINE__); return;}*/

#define return_val_if_fail(p, ret) if(!(p)) {return (ret);}
	/*{fprintf(stderr, "%s:%d Warning: "#p" failed.\n",\
	__func__, __LINE__); return (ret);}*/

#define MAX_ATTR_NR 64
#define server_string(s) {(char* )(s), sizeof(s)-1}
#define to_string(p, s) (p).data = (s); (p).len = sizeof((s)) -1
#define to_string2(p, s) (p)->data = (s); (p)->len = sizeof((s)) -1
#define XWP_SERVER_VER "xwp server 0.1"

typedef struct str_s
{
	char* data; //should be zero terminated.
	size_t len;
}str_t;

struct pool_s;
typedef struct pool_s pool_t;

struct conf_s;
typedef struct conf_s conf_t;

struct upstream_s;
typedef struct upstream_s upstream_t;

struct vhost_loc_conf_s;
typedef struct vhost_loc_conf_s vhost_loc_conf_t;

struct vhost_conf_s;
typedef struct vhost_conf_s vhost_conf_t;

struct module_so_conf_s;
typedef struct module_so_conf_s module_so_conf_t;

struct http_request_s;
typedef struct http_request_s http_request_t;

struct module_s;
typedef struct module_s module_t;

#endif
