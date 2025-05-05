/* Copyright (c) 2025 Topias Silfverhuth
 * SPDX-License-Identifier: 0BSD */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>

#include "util.h"

void*
xrealloc(void *buf, size_t size)
{
	void *ptr;
	ptr = realloc(buf, size);
	if(!ptr){
		perror("realloc");
		exit(-1);
	}
	return ptr;
}

void* 
xmalloc(size_t bytes)
{
	void *ptr;
	ptr = malloc(bytes);
	if(!ptr){
		perror("malloc");
		exit(-1);
	}
	return ptr;
}

void
die(const char* msg)
{
	perror(msg);
	exit(1);
}


int write_all(int fd, void *buf, size_t count)
{
	ssize_t tmp;
	while (count) {

		errno = 0;
		tmp = write(fd, buf, count);
		if (tmp > 0) {
			count -= tmp;
			if (count)
				buf = (void *) ((const char *) buf + tmp);
		} else if (errno != EINTR && errno != EAGAIN)
			return -1;
		if (errno == EAGAIN)	/* Try later, *sigh* */
			usleep(250000);
	}
	return 0;
}

