#ifndef UTIL_H_
#define UTIL_H_
#include <stdlib.h>

#define LEN(X) sizeof((X)) / sizeof((X)[0])

void* xrealloc(void *buf, size_t size);
void* xmalloc(size_t bytes);
void die(const char* msg);

int write_all(int fd, const void *buf, size_t count);
#endif // UTIL_H_
