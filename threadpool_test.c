#include <stdio.h>

#include "threadpool.h"

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

int main(int argc, char **argv)
{
    threadpool_test();
    return 0;
}