#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "hi_errno.h"
#include "hi_gpio.h"
#include "hi_io.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"

#define IR_LEFT_GPIO 14
#define IR_RIGHT_GPIO 13
#define IR_LEFT_IO_NAME WIFI_IOT_IO_NAME_GPIO_14
#define IR_RIGHT_IO_NAME WIFI_IOT_IO_NAME_GPIO_13
#define IR_LEFT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_14_GPIO
#define IR_RIGHT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_13_GPIO

#define UART_PORT WIFI_IOT_UART_IDX_1
#define UART_BAUD_RATE 9600
#define IR_TASK_STACK_SIZE (1024 * 4)
#define IR_TASK_PRIO 25
#define IR_QUEUE_OBJECTS 8
#define IR_SAMPLE_PERIOD_TICKS 50
#define IR_ALIVE_PRINT_TICKS 10

typedef struct {
    hi_gpio_value leftValue;
    hi_gpio_value rightValue;
    unsigned char changed;
    unsigned int leftRet;
    unsigned int rightRet;
} IrSensorMessage;

static osMessageQueueId_t g_irQueue = NULL;
static volatile unsigned char g_sampleIr = 0;

static void UartSendText(const char *text)
{
    (void)UartWrite(UART_PORT, (const unsigned char *)text, strlen(text));
}

static void IrSampleTimerCallback(void *arg)
{
    (void)arg;
    g_sampleIr = 1;
}

static void IrGpioInit(void)
{
    GpioInit();
    IoSetFunc(IR_LEFT_IO_NAME, IR_LEFT_IO_FUNC);
    IoSetFunc(IR_RIGHT_IO_NAME, IR_RIGHT_IO_FUNC);
    (void)hi_gpio_set_dir(HI_GPIO_IDX_14, HI_GPIO_DIR_IN);
    (void)hi_gpio_set_dir(HI_GPIO_IDX_13, HI_GPIO_DIR_IN);
}

static unsigned int UartDeviceInit(void)
{
    WifiIotUartAttribute uartAttr = {
        .baudRate = UART_BAUD_RATE,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_0, WIFI_IOT_IO_FUNC_GPIO_0_UART1_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_1, WIFI_IOT_IO_FUNC_GPIO_1_UART1_RXD);
    return UartInit(UART_PORT, &uartAttr, NULL);
}

static void IrReportStatus(const IrSensorMessage *msg)
{
    const char *prefix = msg->changed ? "IR changed: " : "IR status: ";

    UartSendText(prefix);
    if (msg->leftRet != HI_ERR_SUCCESS || msg->rightRet != HI_ERR_SUCCESS) {
        char text[64];

        (void)snprintf(text, sizeof(text), "read failed, leftRet=0x%x rightRet=0x%x\r\n",
            msg->leftRet, msg->rightRet);
        UartSendText(text);
        printf("%s%s", prefix, text);
        return;
    }

    if (msg->leftValue == HI_GPIO_VALUE1 || msg->rightValue == HI_GPIO_VALUE1) {
        char text[64];

        (void)snprintf(text, sizeof(text), "R=%u L=%u success, signal detected\r\n",
            msg->leftValue, msg->rightValue);
        UartSendText(text);
        printf("%s%s", prefix, text);
    } else {
        char text[64];

        (void)snprintf(text, sizeof(text), "R=%u L=%u waiting, no signal\r\n",
            msg->leftValue, msg->rightValue);
        UartSendText(text);
        printf("%s%s", prefix, text);
    }
}

static void IrSampleTask(void *arg)
{
    osTimerId_t sampleTimer;
    hi_gpio_value currentLeftValue = HI_GPIO_VALUE0;
    hi_gpio_value currentRightValue = HI_GPIO_VALUE0;
    hi_gpio_value lastLeftValue = HI_GPIO_VALUE0;
    hi_gpio_value lastRightValue = HI_GPIO_VALUE0;
    unsigned int aliveTicks = 0;

    (void)arg;

    sampleTimer = osTimerNew(IrSampleTimerCallback, osTimerPeriodic, NULL, NULL);
    if (sampleTimer == NULL) {
        printf("Failed to create infrared timer.\r\n");
        return;
    }

    if (osTimerStart(sampleTimer, IR_SAMPLE_PERIOD_TICKS) != osOK) {
        printf("Failed to start infrared timer.\r\n");
        return;
    }

    UartSendText("Infrared pair timer start.\r\n");
    UartSendText("GPIO14 is left IR, GPIO13 is right IR. HIGH means signal detected.\r\n");
    printf("Infrared pair timer start.\r\n");

    while (1) {
        if (g_sampleIr != 0) {
            IrSensorMessage msg;

            g_sampleIr = 0;
            msg.leftRet = hi_gpio_get_input_val(HI_GPIO_IDX_14, &currentLeftValue);
            msg.rightRet = hi_gpio_get_input_val(HI_GPIO_IDX_13, &currentRightValue);

            msg.leftValue = currentLeftValue;
            msg.rightValue = currentRightValue;
            msg.changed = (currentLeftValue != lastLeftValue || currentRightValue != lastRightValue ||
                msg.leftRet != HI_ERR_SUCCESS || msg.rightRet != HI_ERR_SUCCESS) ? 1 : 0;

            aliveTicks++;
            if (msg.changed != 0 || aliveTicks >= IR_ALIVE_PRINT_TICKS) {
                if (osMessageQueuePut(g_irQueue, &msg, 0U, 0U) != osOK) {
                    printf("IR queue put failed.\r\n");
                }
                aliveTicks = 0;
            }

            lastLeftValue = currentLeftValue;
            lastRightValue = currentRightValue;
        }

        osDelay(1);
    }
}

static void IrPrintTask(void *arg)
{
    (void)arg;

    while (1) {
        IrSensorMessage msg;

        if (osMessageQueueGet(g_irQueue, &msg, NULL, osWaitForever) == osOK) {
            IrReportStatus(&msg);
        }
    }
}

static void CreateIrThread(const char *name, osThreadFunc_t func)
{
    osThreadAttr_t attr;

    attr.name = name;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = IR_TASK_STACK_SIZE;
    attr.priority = IR_TASK_PRIO;

    if (osThreadNew(func, NULL, &attr) == NULL) {
        printf("Failed to create %s.\r\n", name);
    }
}

static void InfraredPairTimerEntry(void)
{
    unsigned int ret;

    g_irQueue = osMessageQueueNew(IR_QUEUE_OBJECTS, sizeof(IrSensorMessage), NULL);
    if (g_irQueue == NULL) {
        printf("Failed to create infrared message queue.\r\n");
        return;
    }

    IrGpioInit();
    ret = UartDeviceInit();
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init UART1, err code: %u\r\n", ret);
        return;
    }

    CreateIrThread("IrSample", IrSampleTask);
    CreateIrThread("IrPrint", IrPrintTask);
}

APP_FEATURE_INIT(InfraredPairTimerEntry);
