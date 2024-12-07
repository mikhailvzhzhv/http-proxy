#include "header.h"


void setup_sockaddr(struct sockaddr_in* s) {
    s->sin_family = AF_INET;
    s->sin_port = htons(PORT);
    s->sin_addr.s_addr = INADDR_ANY;
}


void *handle_client(void *args) {
    int err;

    ThreadArgs *thread_args = (ThreadArgs *)args;
    int client_socket = thread_args->client_socket;
    free(thread_args);

    err = pthread_detach(pthread_self());
    if (err != SUCCESS) {
        fprintf(stderr, "handle_client: pthread_detach() failed: %s\n", strerror(err));
        pthread_exit((void *)EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == -1) {
        perror("handle_client: recv() failed");
        close(client_socket);
        pthread_exit((void *)EXIT_FAILURE);
    }
    buffer[bytes_received] = '\0';

    printf("recv\n");

    // Извлекаем первую строку запроса (метод, URL, HTTP-версия)
    char method[8], url[1024], http_version[16];
    sscanf(buffer, "%s %s %s", method, url, http_version);

    // Проверяем метод (только GET поддерживается)
    if (strcmp(method, "GET") != 0) {
        const char *response = "HTTP/1.0 405 Method Not Allowed\r\n\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        pthread_exit((void *)EXIT_FAILURE);
    }

    // Извлекаем хост и путь из URL
    char host[1024], path[1024];
    if (sscanf(url, "http://%1023[^/]%1023s", host, path) != 2) {
        snprintf(path, sizeof(path), "/");
    }

    printf("host: %s\n", host);
    printf("path: %s\n", path);

    // Подключаемся к целевому серверу
    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;       // Использовать IPv4
    hints.ai_socktype = SOCK_STREAM; // Потоковый сокет

    err = getaddrinfo(host, "http", &hints, &res);
    if (err != 0) {
        fprintf(stderr, "handle_client: getaddrinfo() error: %s\n", gai_strerror(err));
        pthread_exit((void *)EXIT_FAILURE);
    }

    // struct hostent *server = gethostbyname(host);
    // if (server == NULL) {
    //     const char *response = "HTTP/1.0 502 Bad Gateway\r\n\r\n";
    //     send(client_socket, response, strlen(response), 0);
    //     close(client_socket);
    //     return NULL;
    // }

    printf("get host by name!\n");

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        perror("handle_client: socket() failed");
        close(client_socket);
        pthread_exit((void *)EXIT_FAILURE);
    }

    printf("socket!\n");

    err = connect(server_socket, res->ai_addr,res->ai_addrlen);
    if (err != SUCCESS) {
        perror("handle_client: connect() failed");
        close(client_socket);
        close(server_socket);
        pthread_exit((void *)EXIT_FAILURE);
    }

    printf("connect!\n");

    // Формируем запрос к серверу
    snprintf(buffer, sizeof(buffer), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", path, host);
    send(server_socket, buffer, strlen(buffer), 0);

    // Читаем ответ от сервера и перенаправляем клиенту
    while ((bytes_received = recv(server_socket, buffer, sizeof(buffer), 0)) > 0) {
        send(client_socket, buffer, bytes_received, 0);
    }

    close(server_socket);
    close(client_socket);
    freeaddrinfo(res);

    printf("done!\n");

    pthread_exit(EXIT_SUCCESS);
}


int get_client_socket(int server_socket) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    // ECONNREFUSED errno
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


int start_proxy(int server_socket) {
    int err = SUCCESS;
    printf("Proxy server running on port %d...\n", PORT);

    while (1) {
        int client_socket = get_client_socket(server_socket);
        if (client_socket == ERROR) {
            fprintf(stderr, "start_proxy: get_client_socket() failed\n");
            err = ERROR;
            break;
        }
        err = create_client_handler(client_socket);
        if (err != SUCCESS) {
            fprintf(stderr, "start_proxy: create_client_handler() failed\n");
            break;
        }
    }
    close(server_socket);

    return ERROR;
}
