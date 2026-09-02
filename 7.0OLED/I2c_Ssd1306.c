#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "hal_bsp_ssd1306.h"
#include "ohos_init.h"

#define OLED_TASK_STACK_SIZE 1024
#define OLED_TIME_TEXT_SIZE 16
#define OLED_TITLE_TEXT_SIZE 16

static void OledClockTask(void *arg)
{
    uint8_t displayBuff[20] = {0};
    uint8_t hour = 10;
    uint8_t min = 19;
    uint8_t sec = 39;

    (void)arg;

    if (SSD1306_Init() != 0) {
        printf("SSD1306 init failed.\r\n");
        return;
    }

    SSD1306_CLS();
    SSD1306_ShowStr(0, 0, (uint8_t *)"QST CAR", OLED_TITLE_TEXT_SIZE);
    SSD1306_ShowStr(0, 3, (uint8_t *)"2026.08.28", OLED_TITLE_TEXT_SIZE);

    while (1) {
        sec++;
        if (sec > 59) {
            sec = 0;
            min++;
        }

        if (min > 59) {
            min = 0;
            hour++;
        }

        if (hour > 23) {
            hour = 0;
        }

        (void)memset(displayBuff, 0, sizeof(displayBuff));
        (void)sprintf((char *)displayBuff, "%02d:%02d:%02d", hour, min, sec);
        SSD1306_ShowStr(0, 2, displayBuff, OLED_TIME_TEXT_SIZE);

        sleep(1);
    }
}

static void I2cSsd1306Demo(void)
{
    osThreadAttr_t attr;

    attr.name = "I2cSsd1306";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = OLED_TASK_STACK_SIZE;
    attr.priority = osPriorityNormal;

    if (osThreadNew(OledClockTask, NULL, &attr) == NULL) {
        printf("Failed to create I2cSsd1306 task.\r\n");
    }
}

APP_FEATURE_INIT(I2cSsd1306Demo);
