#ifndef POOL_H
#define POOL_H

typedef struct pool_s pool_t;
typedef void (*POOL_CLEANUP_FUNC)(void* ctx);

pool_t* pool_create(int size);
char* pool_alloc(pool_t* thiz, int size);
int pool_calloc(pool_t* thiz, int size);
int pool_add_cleanup(pool_t* thiz, POOL_CLEANUP_FUNC cleanup, void* ctx);

int pool_reset(pool_t* thiz);
int pool_destroy(pool_t* thiz);
char* pool_strdup(pool_t* thiz, char* str);

#endif
