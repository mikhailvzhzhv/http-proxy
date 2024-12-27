#include "header.h"


int main() {
    int err;

    pthread_attr_t attr;
    err = pthread_attr_init(&attr);
    if (err != SUCCESS) {
        fprintf(stderr, "main: pthread_attr_init() failed: %s\n", strerror(err));
        pthread_exit((void *)EXIT_FAILURE);
    }
    pthread_cleanup_push(handle_attr_free, (void *)&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        perror("main: socket() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }
    pthread_cleanup_push(handle_socket_close, (void *)&server_socket);

    struct sockaddr_in server_addr;
    init_sockaddr(&server_addr);

    err = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err != SUCCESS) {
        perror("main: bind() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    err = listen(server_socket, BACKLOG);
    if (err != SUCCESS) {
        perror("main: listen() failed");
        pthread_exit((void *)EXIT_FAILURE);
    }

    printf("Proxy server running on port %d...\n", PORT);

    while (1) {
        int client_socket = get_client_socket(server_socket);
        if (client_socket == ERROR) {
            if (errno == EMFILE) {
                continue;
            }
            fprintf(stderr, "main: get_client_socket() failed\n");
            break;
        }
        err = create_client_handler(client_socket, &attr);
        if (err != SUCCESS) {
            fprintf(stderr, "main: create_client_handler() failed\n");
            close(client_socket);
        }
    }

    printf("main finished\n");

    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);

    return SUCCESS;
}
