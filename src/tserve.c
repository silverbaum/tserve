/* Copyright 2025 Topias Silfverhuth
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#define __linux__ 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <getopt.h>

#include "sock.h"
#include "util.h"
#define LIST_IMPL
#include "list.h"

#include "tserve.h"

#define MAXMSG 8192
#define MAX_EVENTS 1024
#define DEFAULT_BUFSIZE 8192
#define IMAGE_BUFSIZE 524288


#ifdef DEBUG
#define dfprintf fprintf
#endif
#ifndef DEBUG
#define dfprintf(x, y, ...)
#endif

static const char *NOT_FOUND = "HTTP/1.1 404 Not Found\nContent-Length:96\nContent-Type: text/html\n\n\
				<!doctype html><html><head><title>Not Found</title></head><body><h1>Not found</h1></body></html>";

struct list *routes;
struct list *post_routes;
struct list *__route_files;

void
append_route(struct list *l, char *routename, void(*func)(struct request*))
{
	if(!l)
		return;
	struct route *new = xmalloc(sizeof(*new));
	new->name = routename;
	new->func = func;
	append(l, new);
}

struct route *
search_route(struct list *l, char *routename)
{
	struct node *p = l->head;
	while(p != NULL) {
		if (!strcmp(\
			((struct route*)p->data)->name, routename))
				return (struct route*)p->data;

		p = p->next;
	}
	return NULL;
}


struct route_file *
search_route_file(struct list *l, char *routename)
{
	if (!l) {
		return NULL;
	}
	struct node *p = l->head;

	while(p) {
		if(!strcmp(((struct route_file*)p->data)->name, routename)) 
			return (struct route_file*)p->data;
		p = p->next;
	}
	return NULL;
}



void get_mime(const char *file_name, struct route_file *route) {
	char *file_type;
	file_type = strchr(file_name, '.');
	if(!file_type) {
		route->mime = strdup("None");
		return;
	}
	dfprintf(stderr, "extension: %s\n", file_type);

	if(!strcmp(file_type, ".html"))
		route->mime = strdup("text/html");
	else if (!strcmp(file_type, ".js"))
		route->mime = strdup("text/javascript");
	else if (!strcmp(file_type, ".jpg"))
		route->mime = strdup("image/jpg");
	else if (!strcmp(file_type, ".png"))
		route->mime = strdup("image/png");
	else if (!strcmp(file_type, ".txt"))
		route->mime = strdup("text/plain");
	else if (!strcmp(file_type, ".json"))
		route->mime = strdup("application/json");
	else
	 	route->mime = strdup("None");
}

FILE *
open_file(const char *filename)
{
	FILE *fs;
	char path[100];
	if ((fs = fopen(filename, "r")) != NULL)
		return fs;

	snprintf(path, 100, "templates/%s", filename);
	if ((fs=fopen(path, "r")) != NULL)
		return fs;

	snprintf(path, 100, "static/%s", filename);
	if ((fs=fopen(path, "r")) != NULL)
		return fs;


	return NULL;
}

static struct route_file*
_load_file(const char *file, FILE* fs)
{	
	FILE *fstream;
	ssize_t i;
	struct route_file *route = xmalloc(sizeof(*route));

	route->name = strdup(file);
	route->docsize = 0;
	
	get_mime(file, route);
	if(!strncmp(route->mime, "image", 5)) {
		route->buf = xmalloc(IMAGE_BUFSIZE);
		route->bufsize = IMAGE_BUFSIZE;
	} else {
		route->buf = xmalloc(DEFAULT_BUFSIZE);
		route->bufsize = DEFAULT_BUFSIZE;
	}

	if (!fs) {
		fstream = open_file(file);
		if (fstream == NULL) {
			fprintf(stderr, "Failed to open %s\n", file);
			perror(" open");
			return NULL;
		}
	} else {
		fstream = fs;
	}

	
	for(i=0; (fread(&route->buf[i], 1, 1, fstream)) > 0; i++, route->docsize++)
		if(route->docsize+1 >= route->bufsize){
				dfprintf(stderr, "reallocating, docsz: %lu, i: %lu\n", route->docsize, i);
				route->buf = xrealloc(route->buf, route->bufsize*=2);
		}

	fclose(fstream) < 0 ? perror("close") : 0;

	return route;
}

struct route_file*
load_file(const char *file)
{
	return _load_file(file, NULL);
}

struct route_file *
new_route(const char *path, FILE *stream)
{
	struct route_file *f = _load_file(path, stream);
	append(__route_files, f);
	return f;
}

int
get_respond(const struct request *req, const struct route_file *file)
{
	char response[200];
	
	if(!file){
		dfprintf(stderr, "file is NULL\n");
		return -1;
	}

	dfprintf(stderr, "\nroute name: %s, route type: %s route size: %lu, route docsz: %lu\n",
			 file->name, file->mime, file->bufsize, file->docsize);

	snprintf(response, 200, "HTTP/1.1 200 OK\nContent-Length: %lu\nContent-Type: %s\n\n",
			 file->docsize, file->mime);

	if ((write_all(req->client_fd, response, strlen(response))) < 0
	 || (write_all(req->client_fd, file->buf, file->docsize)) < 0) {
		perror("get_response: write");
	}

	dfprintf(stderr, "\nserving client:\n%s\n", response);

	fsync(req->client_fd);
	return 0;
}

int
get_respond_raw(const struct request *req, const char *response, char* mime)
{
	if (!response)
		return -1;

	char http_response[200];

	snprintf(http_response, 200, "HTTP/1.1 200 OK\nContent-Length: %lu\nContent-Type: %s\n\n",
			 strlen(response), mime);

	if (write_all(req->client_fd, http_response, strlen(http_response)) < 0
	|| (write_all(req->client_fd, (void*)response, strlen(response)) < 0)) {
		perror("get_respond_text: write_all");
		return -1;
	}
	
	return 0;
}

int
get_respond_text(const struct request *req, const char *response)
{
	return get_respond_raw(req, response, "text/plain");
}

int
get_respond_json(const struct request *req, const char *response)
{
	return get_respond_raw(req, response, "application/json");
}

/* Matches the HTTP request path with a function/file to serve said path; */
int
serve(struct request *req)
{
	struct route *r;

	if (!strncmp(req->method, "POST", 4)){
		r = search_route(post_routes, req->path);
		if (r) {
			(*r->func)(req);
			return 0;
		} else {
			return -1;
		}
	}

	r = search_route(routes, req->path);
	if (r && r->func) {
		(*r->func)(req);
		return 0;
	}

	struct route_file *rf = search_route_file(__route_files, req->path);
	if (rf) {
		get_respond(req, rf);
		return 0;
	}


	FILE* fstream;
	if ((fstream = open_file(&req->path[1])) != NULL) {
		get_respond(req, new_route(&req->path[1], fstream));
		return 0;
	}

	const int nflen = strlen(NOT_FOUND);
	write(req->client_fd, NOT_FOUND, nflen); 
	dfprintf(stderr, "%s: Not found", req->path);
	return -1;
}

int
process_request(const int filedes, char *buffer)
{
	char *token;

	struct request *req = malloc(sizeof(*req));
	if (!req){
		perror("malloc");
		return -1;
	}
	req->client_fd = filedes;

	char *body = strstr(buffer, "\r\n\r\n");
	if (body){
		dfprintf(stderr, "BODY: %s", body);
	}
	req->body = body ? strdup(body+1) : strdup(buffer);

	const char *delim = " \r\n";
	const char *br = "HTTP/1.1 400 Bad Request";
	
	token = strtok(buffer, delim);
	req->method = strdup(token);
	dfprintf(stderr,"first token: '%s'\n", token);

	token = strtok(NULL, delim);
	dfprintf(stderr, "second token: '%s'\n", token);
	req->path = strdup(token);

	token = strtok(NULL, delim);
	req->protocol = strdup(token);
	dfprintf(stderr, "third token: '%s'\n", token);
	dfprintf(stderr, "method: %s, path: %s, proto: %s\n",
		req->method, req->path, req->protocol);
	if (strncmp(token, "HTTP", 4) != 0)
		goto badrequest;

	return serve(req);

	return 0;

	badrequest:
		write(filedes, br, 25) < 0 ? perror("process_request: write") : 0;
		return -1;
}

int
read_from_client(const int filedes)
{
	char *buffer = (char *)xmalloc(MAXMSG);
	long nbytes;

	nbytes = recv(filedes, buffer, MAXMSG, MSG_WAITALL);
	if (nbytes < 0) {
		/* Read error. */
		perror("read");

	} else if (nbytes == 0) {
		/* End-of-file. */
		free(buffer);
		return -1;
	} else {
		if (nbytes + 1 < MAXMSG)
			buffer[nbytes + 1] = '\0';
		fprintf(stderr, "Server: got message: `%s'\n", buffer);
		if (process_request(filedes, buffer) < 0) {
			free(buffer);
			return 1;
		}
	}

	return 0;
}

int app_init()
{
	//initialize linked lists
	routes = xmalloc(sizeof(*routes));
	post_routes = xmalloc(sizeof(*post_routes));
	__route_files = xmalloc(sizeof(*__route_files));
	if(!routes || !__route_files || !post_routes){
		perror("app_init");
		return -1;
	} else {
		return 0;
	}
}

int
run(unsigned short PORT)
{
	int sock, conn, nfds, epollfd;
	struct epoll_event ev, events[MAX_EVENTS];

	struct sockaddr_in clientname;
	socklen_t addrsize;

	int i;
	
	/* Create the socket and set it up to accept connections. */
	sock = make_socket(PORT);
	if (listen(sock, MAX_EVENTS) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	if ((epollfd = epoll_create1(0)) < 0) {
		perror("epoll_create1");
		exit(EXIT_FAILURE);
	}

	/* events struct is a bit mask to the set "event types"/settings on the socket
	 * EPOLLIN: The fd(sock) is available for read operations.*/
	ev.events = EPOLLIN;
	/* connect the created ipv4 socket to the epoll event object */
	ev.data.fd = sock;
	/* Add the socket to the "interest list" of the epoll file descriptor
	 * according to the settings in the ev epoll_event struct */
	if(epoll_ctl(epollfd, EPOLL_CTL_ADD, sock, &ev) == -1){
		perror("epoll_ctl: sock");
		exit(EXIT_FAILURE);
	}

	fprintf(stderr, "[LOG] Server running on port %d\n", PORT);
	while (1) {
		/* Block until input arrives on one or more active sockets,
		 * similar to select */
		if ((nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1)) < 0){
			perror("epoll_wait");
			exit(EXIT_FAILURE);
		}

		/* Service all the sockets with input pending. */
		for (i = 0; i < nfds; ++i) {
			if (events[i].data.fd == sock){
					addrsize = sizeof(clientname);
					conn = accept(
						sock,
						(struct sockaddr *)&clientname,
						&addrsize);
					if (conn < 0) {
						perror("accept");
						exit(EXIT_FAILURE);
					}
					/* set nonblocking IO on the connected socket which
					 * is necessary for edge-triggered mode (EPOLLET) which
					 * delivers events only when changes occur on the monitored file descriptor */
					fcntl(conn, F_SETFL, O_NONBLOCK);
					ev.events = EPOLLIN | EPOLLET;
					ev.data.fd = conn;
					if(epoll_ctl(epollfd, EPOLL_CTL_ADD, conn, &ev) == -1){
						perror("epoll_ctl: conn");
						exit(EXIT_FAILURE);
					}

					fprintf(stderr,
						"Server: connect from host %s, port %hd.\n",
						inet_ntoa(clientname.sin_addr),
						ntohs(clientname.sin_port));
				} else {
					/* Data arriving on an already-connected socket. */
					if (read_from_client(events[i].data.fd) < 0) {
						close(events[i].data.fd);
						epoll_ctl(epollfd, EPOLL_CTL_DEL, events[i].data.fd, &ev);
					}
				}
		}
	}
}

