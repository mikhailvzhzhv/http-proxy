#include "header.h"
#include "thread_pool.h"


void init_sockaddr(struct sockaddr_in* s) {
    s->sin_family = AF_INET;
    s->sin_port = htons(PORT);
    s->sin_addr.s_addr = INADDR_ANY;
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
    strcat(req_buf, end_of_connection + 2);
}


ssize_t read_all(int socket, char* request) {
    ssize_t total_read = 0;
    while (total_read < BUFFER_SIZE - 1) {
        ssize_t bytes_read = recv(socket, request + total_read, BUFFER_SIZE - total_read - 1, MSG_NOSIGNAL);
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


int recv_all(int *client_socket, int *server_socket) {
    char resp_buf[BUFFER_SIZE] = {0};
    ssize_t bytes_received;
    int err = SUCCESS;

    bytes_received = recv(*server_socket, resp_buf, sizeof(resp_buf), MSG_NOSIGNAL);
    while (bytes_received > 0) {
        err = send(*client_socket, resp_buf, bytes_received, MSG_NOSIGNAL);
        if (err == ERROR) {
            perror("recv_all: send() failed");
            break;
        }
        bytes_received = recv(*server_socket, resp_buf, sizeof(resp_buf), MSG_NOSIGNAL);
        if (bytes_received == ERROR) {
            perror("recv_all: recv() failed");
            err = ERROR;
            break;
        }
    }

    return err;
}


void *handle_client(void *args) {
    int err;
    TaskQueue* task_queue = (TaskQueue*)args;
    if (task_queue == NULL) {
        pthread_exit((void*) EXIT_FAILURE);
    }

    err = pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    if (err != SUCCESS) {
        pthread_exit((void*) EXIT_FAILURE); 
    }

    while (1) {
        Task task = task_queue_get(task_queue);
        int client_socket = task.client_socket;
        if (client_socket == INVALID_SOCKET) {
            continue;
        }

        char request[BUFFER_SIZE] = {0};
        ssize_t bytes_received = read_all(client_socket, request);
        if (bytes_received == ERROR) {
            perror("handle_client: recv() failed");
            close(client_socket);
            continue;
        }

        char url[URL_LEN] = {0};
        char method[METHOD_LEN]  = {0};
        char http_version[HTTP_VERSION_LEN] = {0};
        sscanf(request, "%s %s %s", method, url, http_version);

        if (strcmp(method, "GET") != SUCCESS) {
            const char *response = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
            send(client_socket, response, strlen(response), 0);
            close(client_socket);
            continue;
        }

        char host[HOST_LEN] = {0};
        char path[PATH_LEN] = {0};
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
            close(client_socket);
            continue;
        }

        int server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket == ERROR) {
            perror("handle_client: socket() failed");
            close(client_socket);
            freeaddrinfo(res);
            continue;
        }

        err = connect(server_socket, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        if (err != SUCCESS) {
            perror("handle_client: connect() failed");
            close(client_socket);
            close(server_socket);
            continue;
        }

        char req_buf[BUFFER_SIZE] = {0};
        change_request(req_buf, request);
        err = send(server_socket, req_buf, strlen(req_buf), MSG_NOSIGNAL);
        if (err == ERROR) {
            perror("handle_client: send() failed");
            close(client_socket);
            close(server_socket);
            continue;
        }

        err = recv_all(&client_socket, &server_socket);
        if (err == ERROR) {
            close(client_socket);
            close(server_socket);
            continue;
        }

        close(client_socket);
        close(server_socket);

        printf("cs: %d; done!\n", client_socket);
    }

    pthread_exit((void *)EXIT_SUCCESS);
}
