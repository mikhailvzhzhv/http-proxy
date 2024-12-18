#include "header.h"

int main() {
    int err;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        perror("main: socket() failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    init_sockaddr(&server_addr);

    err = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err != SUCCESS) {
        perror("main: bind() failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    err = listen(server_socket, BACKLOG);
    if (err != SUCCESS) {
        perror("main: listen() failed");
        close(server_socket);
        return EXIT_FAILURE;
    }

    printf("Proxy server running on port %d...\n", PORT);

    while (1) {
        int client_socket = get_client_socket(server_socket);
        printf("main: client_socket %d created\n", client_socket);
        if (client_socket == ERROR) {
            if (errno == EMFILE) {
                printf("errno == EMFILE\n");
                continue;
            }
            fprintf(stderr, "main: get_client_socket() failed\n");
            break;
        }
        err = create_client_handler(client_socket);
        if (err != SUCCESS) {
            fprintf(stderr, "main: create_client_handler() failed\n");
            break;
        }
    }
    err = close(server_socket);
    if (err != SUCCESS) {
        perror("main: close() failed");
    }

    return SUCCESS;
}
