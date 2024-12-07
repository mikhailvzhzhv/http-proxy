#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <netdb.h>
#include <malloc.h>

#define BUFFER_SIZE 8192
#define PORT 80

// Структура для передачи данных в поток
typedef struct {
    int client_socket;
} ThreadArgs;

// Функция для обработки запросов
void *handle_client(void *args) {
    int err;
    ThreadArgs *thread_args = (ThreadArgs *)args;
    int client_socket = thread_args->client_socket;
    free(thread_args);

    printf("get args\n");

    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_received == -1) {
        perror("handle_client: recv() failed");
        close(client_socket);
        return NULL;
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
        return NULL;
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
        fprintf(stderr, "getaddrinfo error: %s\n", gai_strerror(err));
        exit(1);
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
    if (server_socket < 0) {
        perror("handle_client: socket() failed");
        close(client_socket);
        return NULL;
    }

    printf("socket!\n");

    err = connect(server_socket, res->ai_addr,res->ai_addrlen);
    if (err < 0) {
        perror("handle_client: connect() failed");
        close(client_socket);
        close(server_socket);
        return NULL;
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

    return NULL;
}


int main() {
    int err;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("main: socket() failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    err = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err < 0) {
        perror("main: bind() failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    err = listen(server_socket, 10);
    if (err < 0) {
        perror("main: listen() failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Proxy server running on port %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        printf("wait...\n");
        int client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("main: accept() failed");
            break;
        }

        ThreadArgs *args = (ThreadArgs*) malloc(sizeof(ThreadArgs));
        if (args == NULL) {
            fprintf(stderr, "main: malloc() failed\n");
            break;
        }
        args->client_socket = client_socket;
        pthread_t thread;
        err = pthread_create(&thread, NULL, handle_client, args);
        if (err != 0) {
            fprintf(stderr, "main: pthread_create() failed: %s\n", strerror(err));
            break;
        }

        err = pthread_detach(thread);
        if (err != 0) {
            fprintf(stderr, "main: pthread_detach() failed: %s\n", strerror(err));
            break;
        }
    }

    close(server_socket);

    return EXIT_SUCCESS;
}
