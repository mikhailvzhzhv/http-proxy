#pragma once

#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

#define SUCCESS 0
#define ERROR -1
#define THREADS_COUNT 4
#define INVALID_SOCKET -1

void *handle_client(void *args);

typedef struct {
    int client_socket;
} Task;

typedef struct {
    Task tasks[THREADS_COUNT];
    int front;
    int rear;
    int count;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} TaskQueue;

typedef struct {
    pthread_t threads[THREADS_COUNT];
} ThreadPool;

int task_queue_init(TaskQueue* task_queue);
Task task_queue_get(TaskQueue* task_queue);
int task_queue_add(TaskQueue* task_queue, Task task);
int task_queue_destroy(TaskQueue* task_queue);
int thread_pool_init(ThreadPool* thread_pool, TaskQueue* task_queue);
int thread_pool_destroy(ThreadPool* thread_pool);
