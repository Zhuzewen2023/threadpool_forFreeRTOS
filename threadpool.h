#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"
#include "printk.h"

#define MAX_THREADS     64
#define MAX_QUEUE       65536

typedef enum {
    threadpool_ok               = 0,
    threadpool_invalid          = -1,
    threadpool_lock_failure     = -2,
    threadpool_queue_full       = -3,
    threadpool_shutdown         = -4,
    threadpool_thread_failure   = -5,
	threadpool_queue_failure	= -6,
} threadpool_error_t;

typedef enum {
    threadpool_graceful = 1,
} threadpool_destroy_flags_t;

typedef enum {
    immediate_shutdown = 1,
    graceful_shutdown = 2,
} threadpool_shutdown_t;

typedef struct {
    void (*function)(void *);
    void *argument;
} threadpool_task_t;

typedef struct threadpool_t {
    xSemaphoreHandle lock;
    xSemaphoreHandle notify;
    TaskHandle_t *threads;
    threadpool_task_t *queue;
    int thread_count;
    int queue_size;
    int head;
    int tail;
    int count;
    int shutdown;
    int started;
} threadpool_t;

threadpool_t *
threadpool_create(int thread_count, int queue_size);

int
threadpool_add(threadpool_t *pool, void (*function)(void *), void *arg);

int
threadpool_destroy(threadpool_t *pool, int flags);

int
threadpool_free(threadpool_t *pool);

void threadpool_test(void);

#ifdef __cplusplus
}
#endif

#endif
