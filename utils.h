#ifndef UTILS_H
#define UTILS_H
#include "typedef.h"

typedef int (*IS_DELIM_FUNC)(int c);

int open_listen_fd(char* ip, int port);
char* get_token(char** buf, IS_DELIM_FUNC delim_func, const char* delim);
int nwrite(int fd, char* buf, size_t len);
char* read_file(const char* file_name);

const char* http_content_type(const char* extension);
const char* http_status_line(int status);
int http_header_set(array_t* headers, const str_t* name, const str_t* value);
int http_header_equal(array_t* headers, const char* name, const char* value);
const str_t* http_header_str(array_t* headers, const char* name);
int http_header_int(array_t* headers, const char* name);

int buf_create(buf_t* thiz, pool_t* pool, int size);
int array_init(array_t* array, pool_t* pool, int count);
void* array_push(array_t* array);

#endif
