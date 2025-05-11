/* Copyright (c) 2025 Topias Silfverhuth 
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef _TSERVE_H
#define _TSERVE_H

#include "list.h"

#define GET_ROUTE(routename, func) \
		append_route(routes, (routename), (func));

#define POST_ROUTE(routename, func) \
		append_route(post_routes, (routename), (func));
/*
#define PUT_ROUTE(routename, func) \
		append_route(put_routes, (routename), (func));
#define DELETE_ROUTE(routename, func) \
		append_route(delete_routes, (routename), (func));
*/

struct route_file {
	char *name;
	char *mime;
	char *buf;
	size_t bufsize;
	size_t docsize;
};

struct request {
	int client_fd;
	char *method;
	char *path;
	char *protocol;
	char *body;
};

struct route {
	char *name;
	void (*func)(struct request*);
};

extern struct list *routes;
extern struct list *post_routes;

void get_mime(const char *file_name, struct route_file *route);

struct route_file* load_file(const char *file);

void append_route(struct list *l, char *routename, void(*func)(struct request*));

int get_respond(const struct request *req, const struct route_file *file);
int get_respond_raw(const struct request *req, const char *response, char* mime);
int get_respond_text(const struct request *req, const char *response);
int get_respond_json(const struct request *req, const char *response);


int app_init(void);
int run(unsigned short PORT);

#endif
