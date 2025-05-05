#ifndef UTIL_H_
#define UTIL_H_
#include <stdlib.h>

void* xrealloc(void *buf, size_t size);
void* xmalloc(size_t bytes);
void die(const char* msg);

int write_all(int fd, void *buf, size_t count);
#endif // UTIL_H_
