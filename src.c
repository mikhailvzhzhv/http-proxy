#include "header.h"


void init_sockaddr(struct sockaddr_in* s) {
    s->sin_family = AF_INET;
    s->sin_port = htons(PORT);
    s->sin_addr.s_addr = INADDR_ANY;
}


void change_request(char* req_buf, char* request) {
    const char *connection_header = "Connection: ";
    const char *connection_pos = strstr(request, connection_header);
    if (connection_pos == NULL) {
        strcpy(req_buf, request);
        return;
    }
    size_t prefix_len = connection_pos - request;
    strncpy(req_buf, request, prefix_len);
    req_buf[prefix_len] = '\0';
    strcat(req_buf, "Connection: close\r\n");
    const char *end_of_connection = strstr(connection_pos, "\r\n");
    if (end_of_connection != NULL) {
        strcat(req_buf, end_of_connection + 2);
    } else {
        strcat(req_buf, connection_pos + strlen(connection_header));
    }
}


ssize_t read_all(int socket, char* request) {
    ssize_t total_read = 0;
    while (total_read < BUFFER_SIZE - 1) {
        ssize_t bytes_read = recv(socket, request + total_read, BUFFER_SIZE - total_read - 1, 0);
        if (bytes_read <= 0) {
            return bytes_read;
        }

        total_read += bytes_read;
        request[total_read] = '\0';

        if (strstr(request, "\r\n\r\n") != NULL) {
            break;
        }
    }
    return total_read;
}


void handle_socket_close(void* arg) {
    int fd = *(int*) arg;
    close(fd);
}


void handle_addrinfo_free(void* arg) {
    struct addrinfo* res = (struct addrinfo*) arg;
    freeaddrinfo(res);
}


void handle_attr_free(void* arg) {
    pthread_attr_t attr = *(pthread_attr_t*) arg;
    pthread_attr_destroy(&attr);
}


void *handle_client(void *args) {
    int err;

    ThreadArgs *thread_args = (ThreadArgs *)args;
    int client_socket = thread_args->client_socket;
    free(thread_args);

    pthread_cleanup_push(handle_socket_close, (void *)&client_socket);

    char request[BUFFER_SIZE];
    memset(request, 0, sizeof(request));
    ssize_t bytes_received = read_all(client_socket, request);
    if (bytes_received == ERROR) {
        perror("handle_client: recv() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    char method[METHOD_LEN], url[URL_LEN], http_version[HTTP_VERSION_LEN];
    memset(method, 0, sizeof(method));
    memset(url, 0, sizeof(url));
    memset(http_version, 0, sizeof(http_version));
    sscanf(request, "%s %s %s", method, url, http_version);

    if (strcmp(method, "GET") != SUCCESS) {
        const char *response = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        pthread_exit((void *)EXIT_FAILURE);
    }

    char host[HOST_LEN], path[HOST_LEN];
    memset(host, 0, sizeof(host));
    memset(path, 0, sizeof(path));
    if (sscanf(url, "http://%1023[^/]%1023s", host, path) != 2) {
        snprintf(path, sizeof(path), "/");
    }

    printf("cs: %d; host: %s\n", client_socket, host);
    printf("cs: %d; path: %s\n", client_socket, path);
    
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    err = getaddrinfo(host, "http", &hints, &res);
    if (err != SUCCESS) {
        fprintf(stderr, "handle_client: getaddrinfo() error: %s\n", gai_strerror(err));
        pthread_exit((void *)EXIT_FAILURE);
    }

    pthread_cleanup_push(handle_addrinfo_free, (void *)res);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        perror("handle_client: socket() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    pthread_cleanup_push(handle_socket_close, (void *)&server_socket);

    err = connect(server_socket, res->ai_addr,res->ai_addrlen);
    if (err != SUCCESS) {
        perror("handle_client: connect() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    char req_buf[BUFFER_SIZE];
    memset(req_buf, 0, sizeof(req_buf));
    change_request(req_buf, request);
    err = send(server_socket, req_buf, strlen(req_buf), 0);
    if (err == ERROR) {
        perror("handle_client: send() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    char resp_buf[BUFFER_SIZE];
    memset(resp_buf, 0, sizeof(resp_buf));
    bytes_received = recv(server_socket, resp_buf, sizeof(resp_buf), 0);
    while (bytes_received > 0) {
        err = send(client_socket, resp_buf, bytes_received, MSG_NOSIGNAL);
        if (err == ERROR) {
            perror("handle_client: send() failed");
            pthread_exit((void *)EXIT_FAILURE);
        }
        bytes_received = recv(server_socket, resp_buf, sizeof(resp_buf), 0);
    }

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);

    printf("cs: %d; done!\n", client_socket);

    pthread_exit((void *)EXIT_SUCCESS);
}


int get_client_socket(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
    if (client_socket == ERROR) {
        perror("get_client_socket: accept() failed");
    }
    
    return client_socket;
}


int create_client_handler(int client_socket, pthread_attr_t* attr) {
    int err;
    ThreadArgs *args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
    if (args == NULL) {
        fprintf(stderr, "create_client_handler: malloc() failed\n");
        return ERROR;
    }
    args->client_socket = client_socket;
    pthread_t thread;
    err = pthread_create(&thread, attr, handle_client, args);
    if (err != SUCCESS) {
        fprintf(stderr, "create_client_handler: pthread_create() failed: %s\n", strerror(err));
        free(args);
    }

    return err;
}
