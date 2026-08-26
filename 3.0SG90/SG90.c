#include <stdio.h>

#include "cmsis_os2.h"
#include "hi_io.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"

#define SG90_GPIO 2
#define SG90_PWM_PERIOD_US 20000
#define SG90_LEFT_DUTY_US 500
#define SG90_RIGHT_DUTY_US 2500
#define SG90_DUTY_STEP_US 20
#define SG90_PULSE_REPEAT 2
#define SG90_STACK_SIZE (1024 * 4)

static void ServoSweepTask(void);

static void SetAngle(unsigned int duty)
{
    GpioSetOutputVal(SG90_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(duty);

    GpioSetOutputVal(SG90_GPIO, WIFI_IOT_GPIO_VALUE0);
    hi_udelay(SG90_PWM_PERIOD_US - duty);
}

static void HoldAngle(unsigned int duty)
{
    for (int i = 0; i < SG90_PULSE_REPEAT; i++) {
        SetAngle(duty);
    }
}

static void ServoSweepTask(void)
{
    int duty;

    printf("SG90 sweep start.\r\n");

    while (1) {
        printf("SG90 left to right.\r\n");
        for (duty = SG90_LEFT_DUTY_US; duty <= SG90_RIGHT_DUTY_US; duty += SG90_DUTY_STEP_US) {
            HoldAngle((unsigned int)duty);
        }

        printf("SG90 right to left.\r\n");
        for (duty = SG90_RIGHT_DUTY_US; duty >= SG90_LEFT_DUTY_US; duty -= SG90_DUTY_STEP_US) {
            HoldAngle((unsigned int)duty);
        }
    }
}

static void SG90(void)
{
    osThreadAttr_t attr;

    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_2, WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
    GpioSetDir(WIFI_IOT_IO_NAME_GPIO_2, WIFI_IOT_GPIO_DIR_OUT);

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = SG90_STACK_SIZE;

    attr.name = "ServoSweep";
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)ServoSweepTask, NULL, &attr) == NULL) {
        printf("Failed to create ServoSweep!\r\n");
    }
}

APP_FEATURE_INIT(SG90);
