/* Copyright (c) 2025 Topias Silfverhuth 
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef _TSERVE_H
#define _TSERVE_H

#include "list.h"

#define GET_ROUTE(routename, func) \
		append_route(routes, (routename), (func));

struct route {
	char *name;
	void (*func)(int);
};


struct route_file {
	char *name;
	char *mime;
	char *buf;
	size_t bufsize;
	size_t docsize;
};

struct request {
	char *method;
	char *path;
	char *protocol;
};

extern struct list *routes;

void get_mime(const char *file_name, struct route_file *route);

struct route_file* load_file(const char *file);



void append_route(struct list *l, char *routename, void(*func)(int));

int get_respond(const int client_fd, const struct route_file *file);

int app_init(void);
int run(unsigned short PORT);

#endif
