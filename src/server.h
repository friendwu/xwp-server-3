#ifndef SERVER_H
#define SERVER_H
#include "conf.h"

typedef struct server_s server_t;

server_t* server_create(conf_t* conf);
int server_destroy(server_t* thiz);

#endif
