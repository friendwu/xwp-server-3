#ifndef UTILS_H
#define UTILS_H
#include "typedef.h"

typedef int (*IS_DELIM_FUNC)(int c);

int open_listen_fd(char* ip, int port);
char* get_token(char** buf, IS_DELIM_FUNC delim_func, const char* delim);
int nwrite(int fd, char* buf, size_t len);
char* read_file(const char* file_name);

#endif
