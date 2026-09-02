#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"

#define UART_PORT WIFI_IOT_UART_IDX_1
#define MOTOR_TX_GPIO WIFI_IOT_IO_NAME_GPIO_11
#define MOTOR_TX_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_11_GPIO
#define SOFT_UART_BIT_US 104
#define UART_TASK_STACK_SIZE (1024 * 8)
#define UART_TASK_PRIO 25
#define UART_QUEUE_OBJECTS 16
#define UART_BUFF_SIZE 128
#define UART_READ_IDLE_US (10 * 1000)
#define MOTOR_FRAME_REPEAT 3
#define MOTOR_STOP_FRAME_REPEAT 8
#define MOTOR_FRAME_GAP_US 3000
#define CAR_FORWARD_SPEED 150
#define CAR_BACKWARD_SPEED 150

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

static unsigned int MotorUartInit(void)
{
    IoSetFunc(MOTOR_TX_GPIO, MOTOR_TX_IO_FUNC);
    GpioSetDir(MOTOR_TX_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(MOTOR_TX_GPIO, WIFI_IOT_GPIO_VALUE1);
    return WIFI_IOT_SUCCESS;
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

static void MotorSoftUartWriteByte(unsigned char data)
{
    unsigned int i;

    GpioSetOutputVal(MOTOR_TX_GPIO, WIFI_IOT_GPIO_VALUE0);
    hi_udelay(SOFT_UART_BIT_US);

    for (i = 0; i < 8; i++) {
        GpioSetOutputVal(MOTOR_TX_GPIO,
            ((data >> i) & 0x01) ? WIFI_IOT_GPIO_VALUE1 : WIFI_IOT_GPIO_VALUE0);
        hi_udelay(SOFT_UART_BIT_US);
    }

    GpioSetOutputVal(MOTOR_TX_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(SOFT_UART_BIT_US);
}

static void MotorSoftUartWrite(const unsigned char *data, unsigned int len)
{
    unsigned int i;
    int32_t lock;

    lock = osKernelLock();
    for (i = 0; i < len; i++) {
        MotorSoftUartWriteByte(data[i]);
    }
    (void)osKernelRestoreLock(lock);
}

static void Stm32MotorControlRepeat(int leftSpeed, int rightSpeed, unsigned char repeat)
{
    unsigned char frame[6];
    unsigned char leftDir = 0;
    unsigned char rightDir = 0;

    if (leftSpeed < 0) {
        leftDir = 1;
        leftSpeed = -leftSpeed;
    }
    if (rightSpeed < 0) {
        rightDir = 1;
        rightSpeed = -rightSpeed;
    }
    if (leftSpeed > 150) {
        leftSpeed = 150;
    }
    if (rightSpeed > 150) {
        rightSpeed = 150;
    }

    frame[0] = 0xFC;
    frame[1] = leftDir;
    frame[2] = (unsigned char)leftSpeed;
    frame[3] = rightDir;
    frame[4] = (unsigned char)rightSpeed;
    frame[5] = 0xFD;

    for (leftDir = 0; leftDir < repeat; leftDir++) {
        MotorSoftUartWrite(frame, sizeof(frame));
        hi_udelay(MOTOR_FRAME_GAP_US);
    }
}

static void Stm32MotorControl(int leftSpeed, int rightSpeed)
{
    Stm32MotorControlRepeat(leftSpeed, rightSpeed, MOTOR_FRAME_REPEAT);
}

static void CarStop(void)
{
    Stm32MotorControlRepeat(0, 0, MOTOR_STOP_FRAME_REPEAT);
}

static void CarForward(void)
{
    Stm32MotorControl(CAR_FORWARD_SPEED, CAR_FORWARD_SPEED);
}

static void CarLeft(void)
{
    Stm32MotorControl(-50, 150);
}

static void CarRight(void)
{
    Stm32MotorControl(150, -50);
}

static void CarBackward(void)
{
    Stm32MotorControl(-CAR_BACKWARD_SPEED, -CAR_BACKWARD_SPEED);
}

static void ProcessBluetoothCommand(unsigned char cmd)
{
    switch (cmd) {
        case 'O':
            CarStop();
            break;
        case 'W':
            CarForward();
            break;
        case 'A':
            CarLeft();
            break;
        case 'D':
            CarRight();
            break;
        case 'S':
            CarBackward();
            break;
        case 'I':
            Stm32MotorControl(100, 100);
            break;
        case 'K':
            Stm32MotorControl(150, 150);
            break;
        case '\r':
        case '\n':
            return;
        default:
            printf("Unknown bluetooth command: %c\r\n", cmd);
            return;
    }

    printf("Bluetooth command: %c\r\n", cmd);
}

static void UartRecvTask(void *arg)
{
    unsigned char uartBuff[UART_BUFF_SIZE] = {0};

    (void)arg;

    while (1) {
        int readLen = UartRead(UART_PORT, uartBuff, UART_BUFF_SIZE - 1);
        if (readLen > 0) {
            UartMessage msg;
            unsigned int i;

            if (readLen >= UART_BUFF_SIZE) {
                readLen = UART_BUFF_SIZE - 1;
            }

            uartBuff[readLen] = '\0';
            for (i = 0; i < (unsigned int)readLen; i++) {
                ProcessBluetoothCommand(uartBuff[i]);
            }

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

    ret = MotorUartInit();
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init motor soft UART, err code: %u\r\n", ret);
        return;
    }

    CarStop();
    printf("Bluetooth car control start.\r\n");
    CreateUartThread("UartRecv", UartRecvTask);
    CreateUartThread("UartProcess", UartProcessTask);
}

APP_FEATURE_INIT(UartExampleEntry);
