#include <stdio.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"

static void thread1(void);
static void thread2(void);

/*****任务创建*****/
static void Hello_World(void)
{
    osThreadAttr_t attr;

    attr.attr_bits = 0U;   // 设置osThreadJoin是否可以使用
    attr.cb_mem = NULL;    // 控制块指针设置
    attr.cb_size = 0U;     // 控制块指针大小
    attr.stack_mem = NULL; // 任务栈设置
    attr.stack_size = 1024 * 4; // 任务栈大小

    // 创建任务1
    attr.name = "thread1"; // 创建任务名称
    attr.priority = 25;    // 任务优先级
    if (osThreadNew((osThreadFunc_t)thread1, NULL, &attr) == NULL) {
        printf("Failed to create thread1!\n");
    }

    // 创建任务2
    attr.name = "thread2"; // 创建任务名称
    attr.priority = 25;    // 任务优先级
    if (osThreadNew((osThreadFunc_t)thread2, NULL, &attr) == NULL) {
        printf("Failed to create thread2!\n");
    }
}

/*****任务一*****/
static void thread1(void)
{
    while (1) {
        printf("任务1正在运行!\n");
        printf("Hello World!\r\n");
        usleep(1000000); // 延时1s
    }
}

/*****任务二*****/
static void thread2(void)
{
    sleep(1); // 休眠1秒
    while (1) {
        printf("任务2正在运行!\n");
        printf("Hello QST!\r\n");
        usleep(1000000); // 延时1s
    }
}

APP_FEATURE_INIT(Hello_World); // 启动任务
