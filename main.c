#include "header.h"
#include "thread_pool.h"


int main() {
    int err;

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == ERROR) {
        perror("main: socket() failed");
        return ERROR;
    }

    struct sockaddr_in server_addr;
    init_sockaddr(&server_addr);

    err = bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err != SUCCESS) {
        perror("main: bind() failed");
        close(server_socket);
        return ERROR;
    }

    err = listen(server_socket, BACKLOG);
    if (err != SUCCESS) {
        perror("main: listen() failed");
        close(server_socket);
        return ERROR;
    }

    ThreadPool thread_pool;
    TaskQueue task_queue;
    err = task_queue_init(&task_queue);
    if (err != SUCCESS) {
        close(server_socket);
        return ERROR;
    }
    err = thread_pool_init(&thread_pool, &task_queue);
    if (err != SUCCESS) {
        close(server_socket);
        task_queue_destroy(&task_queue);
        return ERROR;
    }

    printf("Proxy server running on port %d...\n", PORT);

    while (1) {
        int client_socket = get_client_socket(server_socket);
        if (client_socket == ERROR) {
            fprintf(stderr, "main: get_client_socket() failed\n");
            break;
        }
        Task task = {client_socket};
        err = task_queue_add(&task_queue, task);
        if (err != SUCCESS) {
            fprintf(stderr, "main: task_queue_add() failed\n");
            break;
        }
    }

    close(server_socket);

    err = thread_pool_destroy(&thread_pool);
    if (err != SUCCESS) {
        fprintf(stderr, "thread_pool_shutdown error\n");
        task_queue_destroy(&task_queue);
        return ERROR;
    }

    err = task_queue_destroy(&task_queue);
    if (err != SUCCESS) {
        fprintf(stderr, "task_queue_destroy error\n");
        return ERROR;
    }

    return SUCCESS;
}
