#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <malloc.h>
#include <errno.h>

#define PORT 80
#define BACKLOG 5

#define BUFFER_SIZE 4096
#define PATH_LEN 1024
#define HOST_LEN 1024
#define METHOD_LEN 8
#define URL_LEN 1024
#define HTTP_VERSION_LEN 10

#define SUCCESS 0
#define ERROR -1

void init_sockaddr(struct sockaddr_in* s);
void *handle_client(void *args);
int get_client_socket(int server_socket);
