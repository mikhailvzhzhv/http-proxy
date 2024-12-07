#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <malloc.h>

#define PORT 80
#define BACKLOG 10

#define BUFFER_SIZE 8192

#define SUCCESS 0
#define ERROR -1

typedef struct {
    int client_socket;
} ThreadArgs;

void setup_sockaddr(struct sockaddr_in* s);
void *handle_client(void *args);
int get_client_socket(int server_socket);
int create_client_handler(int client_socket);
int start_proxy(int server_socket);