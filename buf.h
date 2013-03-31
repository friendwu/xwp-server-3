#ifndef BUF_H
#define BUF_H
typedef struct pool_s pool_t;
typedef struct buf_s
{
	char* start;
	char* end;
	char* pos;
	char* last;
}buf_t;

int buf_create(buf_t* thiz, pool_t* pool, int size);

#endif
