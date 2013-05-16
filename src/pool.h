#ifndef POOL_H
#define POOL_H
typedef struct str_s str_t;
typedef struct pool_s pool_t;
typedef void (*POOL_CLEANUP_FUNC)(void* ctx);

pool_t* pool_create(int size);
void* pool_alloc(pool_t* thiz, int size);
void* pool_calloc(pool_t* thiz, int size);
int pool_add_cleanup(pool_t* thiz, POOL_CLEANUP_FUNC cleanup, void* ctx);

int pool_reset(pool_t* thiz);
int pool_destroy(pool_t* thiz);
char* pool_strdup(pool_t* thiz, char* str);
int pool_strdup2(pool_t* thiz, str_t* out, char* str);
char* pool_strndup(pool_t* thiz, char* str, int len);

#endif
