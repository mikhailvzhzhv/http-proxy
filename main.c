#include "header.h"


int main() {
    int err;
    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        perror("main: socket() failed");
        return EXIT_FAILURE;
    }

    struct sockaddr_in server_addr;
    setup_sockaddr(&server_addr);

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

    err = start_proxy(server_socket);
    if (err != SUCCESS) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
