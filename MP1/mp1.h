#ifndef MP1_H
#define MP1_H

#include <sys/types.h>

ssize_t writen(int fd, const void *buf, size_t n);
ssize_t readline(int fd, void *buf, size_t maxlen);

#endif