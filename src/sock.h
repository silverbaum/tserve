int make_socket(short unsigned int port);
void init_sockaddr(struct sockaddr_in *name,
	      const char *hostname,
	      unsigned int port);

