#include "printk.hpp"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "file_logger.hpp"
#include <stdio.h>
#include <string.h>

char buf[512];

int printk_enabled = 1;
int printk_level = KERN_DEBUG;

static SemaphoreHandle_t printk_lock;

static const char *level_names[] = {
    "[KERN_EMERG]",
    "[KERN_ALERT]",
    "[KERN_CRIT]",
    "[KERN_ERR]",
    "[KERN_WARNING]",
    "[KERN_NOTICE]",
    "[KERN_INFO]",
    "[KERN_DEBUG]",
};

void print_task_info(void)
{
    static char task_list[512]; // 存储任务信息的缓冲区
    vTaskList(task_list);
    printk(KERN_DEBUG,"Task Name    State   Priority   Stack Left   Task Number");
    printk(KERN_DEBUG,"%s", task_list);
	memset(task_list, 0, sizeof(task_list));
    vTaskGetRunTimeStats(task_list);
    printk(KERN_DEBUG,"Task Number  Abs Time  % Time");
    printk(KERN_DEBUG,"%s", task_list);
}

void printk_enable()
{
    printk_enabled = 1;
}

void printk_disable()
{
    printk_enabled = 0;
}

void printk_set_level(int level)
{
    printk_level = level;
}

void printk_init(void)
{
    if(!printk_lock)
        printk_lock = xSemaphoreCreateMutex();
}
void printk(int level, const char *fmt, ...)
{
    va_list args;
    BaseType_t in_isr = xPortIsInsideInterrupt();
    BaseType_t task_woken = pdFALSE;
    
    if(!printk_enabled || level > printk_level)
        return;
    
    if(printk_lock)
    {
	  	in_isr = xPortIsInsideInterrupt();
        if(in_isr)
        {
            if(pdPASS != xSemaphoreTakeFromISR(printk_lock, NULL))
                return;
        }
        else
        {
            xSemaphoreTake(printk_lock, portMAX_DELAY);
        }
    }
    
    va_start(args, fmt);

    printf("%s ", level_names[level]);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);

    if(printk_lock)
    {
	  	in_isr = xPortIsInsideInterrupt();
        if(in_isr)
        {
            xSemaphoreGiveFromISR(printk_lock, &task_woken);
            portYIELD_FROM_ISR(task_woken);
        }
        else
        {
            xSemaphoreGive(printk_lock);
        }
    }
}