/* Copyright 2025 Topias Silfverhuth
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE
#define __linux__ 1

#include <stdio.h>
#include <errno.h>
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

#include <assert.h>
#include "sock.h"
#include "util.h"
#define LIST_IMPL
#include "list.h"

#include "tserve.h"

#define MAXMSG 4096
#define MAX_EVENTS 1024
#define DEFAULT_BUFSIZE 8192
#define IMAGE_BUFSIZE 524288

#define LEN(X) sizeof((X)) / sizeof((X)[0])


#ifdef DEBUG
#define dfprintf fprintf
#endif
#ifndef DEBUG
#define dfprintf(x, y, ...)
#endif

void
append_route(struct list *l, char *routename, void(*func)(int))
{
	if(!l)
		l = xmalloc(sizeof(*l));
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


const char *NOT_FOUND = "HTTP/1.1 404 Not Found\nContent-Length:96\nContent-Type: text/html\n\n\
				<!doctype html><html><head><title>Not Found</title></head><body><h1>Not found</h1></body></html>";

struct list *routes;
struct list *__route_files;


void get_mime(const char *file_name, struct route_file *route) {
	char *file_type;
	file_type = strchr(file_name, '.');
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

struct route_file*
_load_file(const char *file, FILE* fs)
{	
	/* add dynamic file loading runtime? */
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

	assert(route->buf && (route->bufsize > 0) && route->docsize > 0);
	assert(route->mime && route->name);
	return route;
}

struct route_file*
load_file(const char *file){
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
get_respond(const int client_fd, const struct route_file *file)
{
	char response[200];
	
	if(!file){
		return -1;
		dfprintf(stderr, "file is NULL: %d\n", -1);
	}
	assert(file->name);
	assert(file->buf);
	assert(file->bufsize);
	assert(file->mime);

	dfprintf(stderr, "\nroute name: %s, route type: %s route size: %lu, route docsz: %lu\n",
			 file->name, file->mime, file->bufsize, file->docsize);


	snprintf(response, 200, "HTTP/1.1 200 OK\nContent-Length: %lu\nContent-Type: %s\n\n",
			 file->docsize, file->mime);


	if ((write_all(client_fd, response, strlen(response))) < 0
	 || (write_all(client_fd, file->buf, file->docsize)) < 0) {
		perror("get_response: write");
	}

	dfprintf(stderr, "\nserving client:\n%s\n", response);


	fsync(client_fd);
	return 0;
}


/* Matches the HTTP request path with a function/file to serve said path; */
int
serve(struct request req, const int filedes)
{

	struct route *r = search_route(routes, req.path);
	if (r && r->func) {
		(*r->func)(filedes);
		return 0;
	}

	
	struct route_file *rf = search_route_file(__route_files, req.path);
	if (rf) {
		get_respond(filedes, rf);
		return 0;
	}


	FILE* fstream;
	if ((fstream = open_file(&req.path[1])) != NULL) {
		get_respond(filedes, new_route(&req.path[1], fstream));
		return 0;
	}

	const int nflen = strlen(NOT_FOUND);
	write(filedes, NOT_FOUND, nflen); 
	dfprintf(stderr, "Not found");
	return -1;
}

int
process_request(const int filedes, char *buffer)
{
	char *token;

	struct request req;

	const char *delim = " \r\n";
	const char *br = "HTTP/1.1 400 Bad Request";


	token = strtok(buffer, delim);
	req.method = token;
	dfprintf(stderr,"first token: '%s'\n", token);

	token = strtok(NULL, delim);
	dfprintf(stderr, "second token: '%s'\n", token);
	req.path = token;

	token = strtok(NULL, delim);
	req.protocol = token;
	dfprintf(stderr, "third token: '%s'\n", token);
	if (!strncmp(token, "HTTP", 4)){
		return serve(req, filedes);
	} else {
		write(filedes, br, 25) < 0 ? perror("process_request: write") : 0;
		return -1;
	}

	return 0;
}

int
read_from_client(const int filedes)
{
	char *buffer = (char *)xmalloc(MAXMSG);
	long nbytes;

	nbytes = read(filedes, buffer, MAXMSG);
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

static inline void help(const char* arg){
printf("Usage: %s [OPTION] [argument]..\noptions:\n\
-f, --file		choose root file\n\
-p, --port		choose the port to which the socket is bound\n\
-h, --help		display this help information and exit\n", arg);
}

int app_init()
{
	routes = xmalloc(sizeof(*routes));
	__route_files = xmalloc(sizeof(*__route_files));
	if(!routes || !__route_files)
		return -1;
	else
		return 0;
}

int
run(unsigned short PORT)
{
	int sock, conn, nfds, epollfd;
	struct epoll_event ev, events[MAX_EVENTS];

	struct sockaddr_in clientname;
	socklen_t addrsize;

	int i;

	if (!PORT)
		PORT = 8000;

	
	/* Read files to memory */
	



	/* Create the socket and set it up to accept connections. */
	sock = make_socket(PORT);
	if (listen(sock, 1) < 0) {
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

