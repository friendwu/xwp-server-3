#ifndef UTILS_H
#define UTILS_H
#include <stdint.h>
#include "typedef.h"

typedef int (*IS_DELIM_FUNC)(int c);

int open_listen_fd(char* ip, int port);
int get_token(str_t* str, char** buf, IS_DELIM_FUNC delim_func, const char* delim);
int nwrite(int fd, char* buf, size_t len);
char* read_file(const char* file_name);
void uint16_little_endian(char* buf, uint16_t data);

#endif
