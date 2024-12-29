#include "thread_pool.h"


int task_queue_init(TaskQueue* task_queue) {
    int err;
    if (task_queue == NULL) {
        return ERROR;
    }

    task_queue->front = 0;
    task_queue->rear = 0;
    task_queue->count = 0;

    err = pthread_mutex_init(&task_queue->lock, NULL);
    if (err != SUCCESS) {
        return err;
    }

    err = pthread_cond_init(&task_queue->cond, NULL);
    if (err != SUCCESS) {
        pthread_mutex_destroy(&task_queue->lock);
        return err;
    }
    
    return SUCCESS;
}


int task_queue_add(TaskQueue* task_queue, Task task) {
    int err;
    if (task_queue == NULL) {
        return ERROR;
    }

    err = pthread_mutex_lock(&task_queue->lock);
    if (err != SUCCESS) {
        return err;
    }

    while (task_queue->count == THREADS_COUNT) {
        err = pthread_cond_wait(&task_queue->cond, &task_queue->lock);
        if (err != SUCCESS) {
            pthread_mutex_unlock(&task_queue->lock);
            return err;
        }
    }

    task_queue->tasks[task_queue->rear] = task;
    task_queue->rear = (task_queue->rear + 1) % THREADS_COUNT;
    task_queue->count++;

    err = pthread_cond_broadcast(&task_queue->cond);
    if (err != SUCCESS) {
        pthread_mutex_unlock(&task_queue->lock);
        return err;
    }

    err = pthread_mutex_unlock(&task_queue->lock);
    if (err != SUCCESS) {
        return err;
    }

    return SUCCESS;
}


Task task_queue_get(TaskQueue* task_queue) {
    Task t = {INVALID_SOCKET};
    if (task_queue == NULL) {
        return t;
    }

    int err;
    err = pthread_mutex_lock(&task_queue->lock);
    if (err != SUCCESS) {
        fprintf(stderr, "pthread_mutex_lock err\n");
        return t;
    }

    while (task_queue->count == 0) {
        int oldstate;
        err = pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &oldstate);
        if (err != SUCCESS) {
            fprintf(stderr, "pthread_setcancelstate err\n");
            return t;
        }
        err = pthread_cond_wait(&task_queue->cond, &task_queue->lock);
        if (err != SUCCESS) {
            fprintf(stderr, "task_queue_get: pthread_cond_wait() err\n");
            pthread_mutex_unlock(&task_queue->lock);
            return t;
        }
        pthread_setcancelstate(oldstate, NULL);
        if (err != SUCCESS) {
            fprintf(stderr, "pthread_setcancelstate err\n");
            return t;
        }
    }

    Task task = task_queue->tasks[task_queue->front];
    task_queue->front = (task_queue->front + 1) % THREADS_COUNT;
    task_queue->count--;

    err = pthread_cond_broadcast(&task_queue->cond);
    if (err != SUCCESS) {
        close(task.client_socket);
        fprintf(stderr, "pthread_cond_broadcast err\n");
        pthread_mutex_unlock(&task_queue->lock);
        return t;
    }

    err = pthread_mutex_unlock(&task_queue->lock);
    if (err != SUCCESS) {
        close(task.client_socket);
        fprintf(stderr, "pthread_mutex_unlock err\n");
        return t;
    }
    
    return task;
}


int task_queue_destroy(TaskQueue* task_queue) {
    int err;
    if (task_queue == NULL) {
        return ERROR;
    }
    err = pthread_mutex_lock(&task_queue->lock);
    if (err != SUCCESS) {
        fprintf(stderr, "task_queue_destroy: pthread_mutex_lock() err\n");
        return ERROR;
    }

    for (int i = 0; i < task_queue->count; ++i) {
        close(task_queue->tasks[(task_queue->front + i) % THREADS_COUNT].client_socket);
    }

    task_queue->front = 0;
    task_queue->rear = 0;
    task_queue->count = 0;

    err = pthread_cond_destroy(&task_queue->cond);
    if (err != SUCCESS) {
        fprintf(stderr, "task_queue_destroy: pthread_cond_destroy() err\n");
        pthread_mutex_unlock(&task_queue->lock);
        return ERROR;
    }

    err = pthread_mutex_unlock(&task_queue->lock);
    if (err != SUCCESS) {
        fprintf(stderr, "task_queue_destroy: pthread_mutex_unlock() err\n");
        return ERROR;
    }

    return SUCCESS;
}

int thread_pool_init(ThreadPool* thread_pool, TaskQueue* task_queue) {
    int err;
    if (thread_pool == NULL || task_queue == NULL) {
        return ERROR;
    }

    pthread_attr_t attr;
    err = pthread_attr_init(&attr);
    if (err != SUCCESS) {
        return ERROR;
    }
    
    for (int i = 0; i < THREADS_COUNT; i++) {
        err = pthread_create(&thread_pool->threads[i], &attr, handle_client, task_queue);
        if (err != SUCCESS) {
            pthread_attr_destroy(&attr);
            return ERROR;
        }
    }
    err = pthread_attr_destroy(&attr);
    if (err != SUCCESS) {
        return ERROR;
    }

    return SUCCESS;  
}


int thread_pool_destroy(ThreadPool* thread_pool) {
    int err;
    if (thread_pool == NULL) {
        return ERROR;
    }
    for (int i = 0; i < THREADS_COUNT; i++) {
        err = pthread_cancel(thread_pool->threads[i]);
        if (err != SUCCESS) {
            fprintf(stderr, "pthread_cancel err\n");
            return ERROR;
        }
    }

    return SUCCESS;
}
