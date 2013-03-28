#ifndef POOL_H
#define POOL_H

typedef struct pool_s pool_t;

pool_t* pool_create(int size);
char* pool_alloc(pool_t* thiz, int size);
int pool_calloc(pool_t* thiz, int size);
int pool_reset(pool_t* thiz);
int pool_destroy(pool_t* thiz);

#endif
