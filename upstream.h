#ifndef UPSTREAM_H
#define UPSTREAM_H
#define HTTP_UPSTREAM_DONE 1
#define HTTP_UPSTREAM_FAIL 0

typedef struct upstream_s
{
	char priv[1];
}upstream_t;

static inline int upstream_process(upstream_t* thiz){return 1;}
static inline int upstream_destroy(upstream_t* thiz){return 1;}

#endif
