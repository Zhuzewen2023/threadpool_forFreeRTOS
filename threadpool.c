#include <stdlib.h>
#include <stdio.h>
#include "threadpool.h"
#include "printk.h"

static void threadpool_thread(void *arg);

threadpool_t *
threadpool_create(int thread_count, int queue_size)
{
    threadpool_t *pool = NULL;
    int i;

    if (thread_count <= 0 || thread_count > MAX_THREADS || queue_size <= 0
        || queue_size > MAX_QUEUE) {
            return NULL;
        }

    pool = 
    (threadpool_t *)pvPortMalloc(sizeof(threadpool_t));
    
    if (pool == NULL) {
        goto err;
    }

    pool->thread_count = 0;
    pool->queue_size = queue_size;
    pool->head = pool->tail = pool->count = 0;
    pool->shutdown = pool->started = 0;

    pool->threads = 
    (TaskHandle_t *)pvPortMalloc(thread_count * sizeof(TaskHandle_t));
    
    if (pool->threads == NULL) {
        goto err;
    }

    pool->queue = 
    (threadpool_task_t *)pvPortMalloc(queue_size * sizeof(threadpool_task_t));

    if (pool->queue == NULL) {
        goto err;
    }

    pool->lock = xSemaphoreCreateBinary();
    xSemaphoreGive(pool->lock);

    if (pool->lock == NULL) {
        goto err;
    }

    pool->notify = xSemaphoreCreateCounting(queue_size, 0);

    if (pool->notify == NULL) {
        goto err;
    }

    for (i = 0; i < thread_count; i++) {
        if (pdFALSE == xTaskCreate(
            threadpool_thread, 
            "threadpool_worker", 
            configMINIMAL_STACK_SIZE * 4, 
            (void *)pool, 
            tskIDLE_PRIORITY + 1, &pool->threads[i])) {
                printf("task create failed %d\n", i);
            goto err;
            }

        vTaskSetTaskNumber(pool->threads[i], i);
        pool->thread_count++;
        pool->started++;
    }

    return pool;


err:
    if (pool) {
        threadpool_free(pool);
    }
    return NULL;
}

int
threadpool_add(threadpool_t *pool, void (*function)(void *), void *arg)
{
    int err = 0;
    int next;

    if (pool == NULL || function == NULL) {
        return threadpool_invalid;
    }

    if (xSemaphoreTake(pool->lock, portMAX_DELAY) != pdTRUE) {
        return threadpool_lock_failure;
    }

    next = (pool->tail + 1) % pool->queue_size;

    do {
        if (pool->count == pool->queue_size) {
            err = threadpool_queue_full;
            break;
        }

        if (pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }

        pool->queue[pool->tail].function = function;
        pool->queue[pool->tail].argument = arg;
        pool->tail = next;
        pool->count += 1;

        
        if (xSemaphoreGive(pool->notify) != pdTRUE) {
            err = threadpool_lock_failure;
            break;
        }

    } while(0);

    if (xSemaphoreGive(pool->lock) != pdTRUE) {
        return threadpool_lock_failure;
    }

    return err;
}

int
threadpool_destroy(threadpool_t *pool, int flags)
{
    int i, err = 0;

    if (pool == NULL) {
        return threadpool_invalid;
    }

    if (xSemaphoreTake(pool->lock, portMAX_DELAY) != pdTRUE) {
        return threadpool_lock_failure;
    }

    do {
        if (pool->shutdown) {
            err = threadpool_shutdown;
            break;
        }
        pool->shutdown = (flags & threadpool_graceful) ? 
            graceful_shutdown : immediate_shutdown;

        xSemaphoreGive(pool->lock);

        for (int i = 0; i < pool->thread_count; i++) {
            xSemaphoreGive(pool->notify);
        }

        while (pool->started > 0) {
            xSemaphoreGive(pool->notify);
            vTaskDelay(pdMS_TO_TICKS(10));
        }


    } while(0);
        

    if (!err) {
        threadpool_free(pool);
    }

    return err;
}

int
threadpool_free(threadpool_t *pool)
{
    if (pool == NULL || pool->started > 0) {
        return -1;
    }

    if (pool->threads) {
        vPortFree(pool->threads);
    }

    if (pool->queue) {
        vPortFree(pool->queue);
    }

    if (pool->lock) {
        vSemaphoreDelete(pool->lock);
    }

    if (pool->notify) {
        vSemaphoreDelete(pool->notify);
    }

    vPortFree(pool);

    return 0;

}

static void threadpool_thread(void *arg)
{
    threadpool_t *pool = (threadpool_t *)arg;
    threadpool_task_t task;

    for (;;) {
        xSemaphoreTake(pool->notify, portMAX_DELAY);
        /*获取锁访问共享资源 */
        xSemaphoreTake(pool->lock, portMAX_DELAY);

        if (pool->shutdown == immediate_shutdown) {
            break;
        }

        if ((pool->shutdown == graceful_shutdown) && (pool->count == 0)) {
            break;
        }

        task.function = pool->queue[pool->head].function;
        task.argument = pool->queue[pool->head].argument;
        pool->head = (pool->head + 1) % pool->queue_size;
        pool->count -= 1;

        xSemaphoreGive(pool->lock);

        /*执行任务 */
        (*(task.function))(task.argument);
    }
    pool->started--;
    xSemaphoreGive(pool->lock);
    vTaskDelete(NULL);
}

#define THREAD  10
#define TASKS   100

void dummy_task(void *arg)
{
    int* pi = (int *)arg;
    *pi += 1;
    UBaseType_t task_num = uxTaskGetTaskNumber(xTaskGetCurrentTaskHandle());
    printk(KERN_DEBUG, "[Task%02u] Dummy task %d\n", task_num, *pi);
    free(pi);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void threadpool_test_graceful()
{
    int i;

    threadpool_t * tp = threadpool_create(THREAD, TASKS);
    if (tp == NULL) {
        printf("Failed to create threadpool\n");
        return;
    }
    for (i = 0; i < TASKS; i++) {
        int* pi = malloc(sizeof(int));
        *pi = i;
        printf("Add task %d\n", *pi);
        if(threadpool_add(tp, dummy_task, (void *)pi)) {
            printf("Failed to add task %d\n", i);
            break;
        }
    }

    threadpool_destroy(tp, 1);

}

void threadpool_test_immediate()
{
    int i;

    threadpool_t * tp = threadpool_create(THREAD, TASKS);
    if (tp == NULL) {
        printf("Failed to create threadpool\n");
        return;
    }
    for (i = 0; i < TASKS; i++) {
        int* pi = malloc(sizeof(int));
        *pi = i;
        if(threadpool_add(tp, dummy_task, (void *)pi)) {
            printf("Failed to add task %d\n", i);
            break;
        }
    }

    vTaskDelay(pdMS_TO_TICKS(3000));
    threadpool_destroy(tp, 0);
}

void threadpool_test(void)
{
    printf("***************threadpool_test graceful start************\n");
    threadpool_test_graceful();
    printf("***************threadpool_test_graceful end************\n");
    print_task_info();
    printf("***************threadpool_test_graceful end************\n");

    printf("***************threadpool_test_immediate start************\n");
    threadpool_test_immediate();
    vTaskDelay(pdMS_TO_TICKS(1000));
    printf("***************threadpool_test_immediate end************\n");
    print_task_info();
    printf("***************threadpool_test_immediate end************\n");
}