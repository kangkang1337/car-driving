#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"

#define UART_PORT WIFI_IOT_UART_IDX_1
#define UART_TASK_STACK_SIZE (1024 * 8)
#define UART_TASK_PRIO 25
#define UART_QUEUE_OBJECTS 16
#define UART_BUFF_SIZE 128
#define UART_READ_IDLE_US (200 * 1000)

typedef struct {
    unsigned int len;
    unsigned char data[UART_BUFF_SIZE];
} UartMessage;

static osMessageQueueId_t g_uartQueue = NULL;

static void UartGpioInit(void)
{
    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_0, WIFI_IOT_IO_FUNC_GPIO_0_UART1_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_1, WIFI_IOT_IO_FUNC_GPIO_1_UART1_RXD);
}

static unsigned int UartDeviceInit(void)
{
    WifiIotUartAttribute uartAttr = {
        .baudRate = 9600,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    UartGpioInit();
    return UartInit(UART_PORT, &uartAttr, NULL);
}

static void UartRecvTask(void *arg)
{
    unsigned char uartBuff[UART_BUFF_SIZE] = {0};

    (void)arg;

    while (1) {
        int readLen = UartRead(UART_PORT, uartBuff, UART_BUFF_SIZE - 1);
        if (readLen > 0) {
            UartMessage msg;

            if (readLen >= UART_BUFF_SIZE) {
                readLen = UART_BUFF_SIZE - 1;
            }

            uartBuff[readLen] = '\0';
            msg.len = (unsigned int)readLen;
            (void)memset(msg.data, 0, sizeof(msg.data));
            (void)memcpy(msg.data, uartBuff, (unsigned int)readLen);

            if (osMessageQueuePut(g_uartQueue, &msg, 0U, 0U) != osOK) {
                printf("UART queue put failed.\r\n");
            }
        }

        usleep(UART_READ_IDLE_US);
    }
}

static void UartProcessTask(void *arg)
{
    (void)arg;

    while (1) {
        UartMessage msg;
        osStatus_t status = osMessageQueueGet(g_uartQueue, &msg, NULL, osWaitForever);
        if (status == osOK) {
            printf("UART recv: %s\r\n", msg.data);
            (void)UartWrite(UART_PORT, msg.data, msg.len);
            (void)UartWrite(UART_PORT, (const unsigned char *)"\r\n", 2);
        }
    }
}

static void CreateUartThread(const char *name, osThreadFunc_t func)
{
    osThreadAttr_t attr;

    attr.name = name;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = UART_TASK_STACK_SIZE;
    attr.priority = UART_TASK_PRIO;

    if (osThreadNew(func, NULL, &attr) == NULL) {
        printf("Failed to create %s.\r\n", name);
    }
}

static void UartExampleEntry(void)
{
    unsigned int ret;

    g_uartQueue = osMessageQueueNew(UART_QUEUE_OBJECTS, sizeof(UartMessage), NULL);
    if (g_uartQueue == NULL) {
        printf("Failed to create UART message queue.\r\n");
        return;
    }

    ret = UartDeviceInit();
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init UART1, err code: %u\r\n", ret);
        return;
    }

    printf("UART1 example start.\r\n");
    CreateUartThread("UartRecv", UartRecvTask);
    CreateUartThread("UartProcess", UartProcessTask);
}

APP_FEATURE_INIT(UartExampleEntry);
