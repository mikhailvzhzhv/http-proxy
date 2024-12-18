#include "header.h"


void init_sockaddr(struct sockaddr_in* s) {
    s->sin_family = AF_INET;
    s->sin_port = htons(PORT);
    s->sin_addr.s_addr = INADDR_ANY;
}


ssize_t read_all(int socket, char* buffer) {
    ssize_t total_read = 0;
    while (total_read < BUFFER_SIZE - 1) {
        ssize_t bytes_read = recv(socket, buffer + total_read, BUFFER_SIZE - total_read - 1, 0);
        if (bytes_read <= 0) {
            return bytes_read;
        }

        total_read += bytes_read;
        buffer[total_read] = '\0';

        if (strstr(buffer, "\r\n\r\n") != NULL) {
            break;
        }
    }
    return total_read;
}


void handle_socket_close(void* arg) {
    int fd = *(int*) arg;
    printf("handler %d\n", fd);
    int err = close(fd);
    if (err != SUCCESS) {
        printf("handle_socket_close err\n");
    }
}


void *handle_client(void *args) {
    int err;
    int res;

    ThreadArgs *thread_args = (ThreadArgs *)args;
    int client_socket = thread_args->client_socket;
    free(thread_args);

    pthread_cleanup_push(handle_socket_close, (void *)&client_socket);

    err = pthread_detach(pthread_self());
    if (err != SUCCESS) {
        fprintf(stderr, "handle_client: pthread_detach() failed: %s\n", strerror(err));
        pthread_exit((void *)EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_received = read_all(client_socket, buffer);
    if (bytes_received == ERROR) {
        perror("handle_client: recv() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    char method[METHOD_LEN], url[URL_LEN], http_version[HTTP_VERSION_LEN];
    res = sscanf(buffer, "%s %s %s", method, url, http_version);
    if (res < 0) {
        fprintf(stderr, "res = %d\n", res);
        perror("handle_client: sscanf() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    err = strcmp(method, "GET");
    if (err != SUCCESS) {
        const char *response = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
        err = send(client_socket, response, strlen(response), 0);
        if (err == ERROR) {
            perror("handle_client: send() failed");
        }
        pthread_exit((void *)EXIT_FAILURE);
    }

    char host[HOST_LEN], path[PATH_LEN];
    res = sscanf(url, "http://%1023[^/]%1023s", host, path);
    if (res != 2) {
        snprintf(path, sizeof(path), "/");
    }

    printf("cs: %d; host: %s\n", client_socket, host);
    printf("cs: %d; path: %s\n", client_socket, path);

    // Подключаемся к целевому серверу
    struct addrinfo hints, *addr_info;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    
    err = getaddrinfo(host, "http", &hints, &addr_info);
    if (err != SUCCESS) {
        fprintf(stderr, "handle_client: getaddrinfo() error: %s\n", gai_strerror(err));
        pthread_exit((void *)EXIT_FAILURE);
    }

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        perror("handle_client: socket() failed");
        freeaddrinfo(addr_info);
        pthread_exit((void *)EXIT_FAILURE);
    }
    pthread_cleanup_push(handle_socket_close, (void *)&server_socket);

    err = connect(server_socket, addr_info->ai_addr, addr_info->ai_addrlen);
    if (err != SUCCESS) {
        perror("handle_client: connect() failed");
        freeaddrinfo(addr_info);
        pthread_exit((void *)EXIT_FAILURE);
    }

    // Формируем запрос к серверу
    char req_buf[BUFFER_SIZE];
    snprintf(req_buf, sizeof(req_buf), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    err = send(server_socket, req_buf, strlen(req_buf), 0);
    if (err == ERROR) {
        perror("handle_client: send() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    // Читаем ответ от сервера и перенаправляем клиенту
    char resp_buf[BUFFER_SIZE];
    bytes_received = recv(server_socket, resp_buf, sizeof(resp_buf), 0);
    while (bytes_received > 0) {
        err = send(client_socket, resp_buf, bytes_received, MSG_NOSIGNAL);
        if (err == ERROR) {
            perror("handle_client: send() failed");
            pthread_exit((void *)EXIT_FAILURE);
        }
        bytes_received = recv(server_socket, resp_buf, sizeof(resp_buf), 0);
    }

    pthread_cleanup_pop(SUCCESS);
    pthread_cleanup_pop(SUCCESS);

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


int create_client_handler(int client_socket) {
    int err;
    ThreadArgs *args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
    if (args == NULL) {
        fprintf(stderr, "create_client_handler: malloc() failed\n");
        return ERROR;
    }
    args->client_socket = client_socket;
    pthread_t thread;
    err = pthread_create(&thread, NULL, handle_client, args);
    if (err != SUCCESS) {
        fprintf(stderr, "create_client_handler: pthread_create() failed: %s\n", strerror(err));
        free(args);
    }

    return err;
}
