#ifndef UPSTREAM_H
#define UPSTREAM_H
#define HTTP_UPSTREAM_DONE 1
#define HTTP_UPSTREAM_FAIL 0

typedef struct upstream_s
{
	char priv[1];
}upstream_t;

int upstream_process(upstream_t* thiz);

#endif
