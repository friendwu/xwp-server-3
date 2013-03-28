#ifndef SERVER_H
#define SERVER_H
#include "typedef.h"

struct _Server;
typedef struct _Server Server;
//typedef struct _Module Module;

Server* server_create(const char* config_file);
//Ret server_handle_req(Server* thiz, HttpReq* req, HttpResp* resp);
//Module* server_get_module(Server* thiz, const char* name, const ModuleParam* params);
Ret server_destroy(Server* thiz);

#endif
