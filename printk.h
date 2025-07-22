#ifndef __PRINTK_HPP__
#define __PRINTK_HPP__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdarg.h>

#define KERN_EMERG      0
#define KERN_ALERT      1
#define KERN_CRIT       2
#define KERN_ERR        3
#define KERN_WARNING    4
#define KERN_NOTICE     5
#define KERN_INFO       6
#define KERN_DEBUG      7

extern int printk_enabled;
extern int printk_level;

void printk_init(void);
void printk_enable();
void printk_disable();
void printk_set_level(int level);
void printk(int level, const char *fmt, ...);
void print_task_info(void);

#ifdef  __cplusplus
}
#endif

#endif
