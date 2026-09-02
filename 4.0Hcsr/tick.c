#include <stdio.h>

#include "cmsis_os2.h"
#include "hi_io.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"

#define HCSR04_TRIG_GPIO 7
#define HCSR04_ECHO_GPIO 8
#define HCSR04_STACK_SIZE (1024 * 4)
#define HCSR04_TASK_DELAY_TICKS 1
#define HCSR04_MEASURE_PERIOD_TICKS 300
#define TICK_PRINT_PERIOD_TICKS 100
#define HCSR04_ECHO_TIMEOUT_US 30000

static volatile unsigned char g_measureDistance = 0;
static volatile unsigned char g_printTick = 0;

static void Hcsr04MeasureTimerCallback(void *arg)
{
    (void)arg;
    g_measureDistance = 1;
}

static void TickPrintTimerCallback(void *arg)
{
    (void)arg;
    g_printTick = 1;
}

static void Hcsr04GpioInit(void)
{
    GpioInit();

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_8, WIFI_IOT_IO_FUNC_GPIO_8_GPIO);

    GpioSetDir(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetDir(HCSR04_ECHO_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);
}

static unsigned int WaitEchoValue(WifiIotGpioValue expectValue, unsigned int timeoutUs)
{
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;
    unsigned int start = hi_get_us();

    while ((unsigned int)(hi_get_us() - start) < timeoutUs) {
        GpioGetInputVal(HCSR04_ECHO_GPIO, &value);
        if (value == expectValue) {
            return hi_get_us();
        }
    }

    return 0;
}

static float GetDistance(void)
{
    unsigned int startTime;
    unsigned int endTime;
    unsigned int echoTime;

    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(20);
    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    startTime = WaitEchoValue(WIFI_IOT_GPIO_VALUE1, HCSR04_ECHO_TIMEOUT_US);
    if (startTime == 0) {
        return -1.0f;
    }

    endTime = WaitEchoValue(WIFI_IOT_GPIO_VALUE0, HCSR04_ECHO_TIMEOUT_US);
    if (endTime == 0) {
        return -2.0f;
    }

    echoTime = endTime - startTime;
    return (float)echoTime * 0.034f / 2.0f;
}

static void Hcsr04TickTask(void *arg)
{
    osTimerId_t measureTimer;
    osTimerId_t tickPrintTimer;

    (void)arg;
    Hcsr04GpioInit();

    measureTimer = osTimerNew(Hcsr04MeasureTimerCallback, osTimerPeriodic, NULL, NULL);
    tickPrintTimer = osTimerNew(TickPrintTimerCallback, osTimerPeriodic, NULL, NULL);
    if (measureTimer == NULL || tickPrintTimer == NULL) {
        printf("Failed to create HCSR04 timers.\r\n");
        return;
    }

    osTimerStart(measureTimer, HCSR04_MEASURE_PERIOD_TICKS);
    osTimerStart(tickPrintTimer, TICK_PRINT_PERIOD_TICKS);

    printf("HCSR04 tick timer start.\r\n");

    while (1) {
        if (g_measureDistance != 0) {
            float distance;

            g_measureDistance = 0;
            distance = GetDistance();
            if (distance == -1.0f) {
                printf("distance timeout: echo never high.\r\n");
            } else if (distance == -2.0f) {
                printf("distance timeout: echo never low.\r\n");
            } else {
                printf("distance is %.1f cm\r\n", distance);
            }
        }

        if (g_printTick != 0) {
            g_printTick = 0;
            printf("current tick: %u\r\n", hi_get_tick());
        }

        osDelay(HCSR04_TASK_DELAY_TICKS);
    }
}

static void Hcsr04TickEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "Hcsr04Tick";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = HCSR04_STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(Hcsr04TickTask, NULL, &attr) == NULL) {
        printf("Failed to create Hcsr04Tick task.\r\n");
    }
}

APP_FEATURE_INIT(Hcsr04TickEntry);
