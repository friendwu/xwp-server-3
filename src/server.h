#ifndef SERVER_H
#define SERVER_H

typedef struct server_s server_t;

server_t* server_create(const char* config_file);
int server_destroy(server_t* thiz);

#endif
