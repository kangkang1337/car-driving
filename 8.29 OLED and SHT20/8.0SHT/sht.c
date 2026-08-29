#include <stdio.h>
#include <string.h>

#include "cmsis_os2.h"
#include "hal_bsp_sht20.h"
#include "hal_bsp_ssd1306.h"
#include "ohos_init.h"

#define SHT20_TASK_STACK_SIZE (1024 * 4)
#define SHT20_TASK_PRIO 25
#define SHT20_SAMPLE_PERIOD_TICKS 300
#define DISPLAY_TEXT_SIZE 16

static osSemaphoreId_t g_sampleSem = NULL;

static void SampleSignalTask(void *arg)
{
    (void)arg;

    while (1) {
        osSemaphoreRelease(g_sampleSem);
        osDelay(SHT20_SAMPLE_PERIOD_TICKS);
    }
}

static void Sht20DisplayTask(void *arg)
{
    float temperature = 0.0f;
    float humidity = 0.0f;
    char tempLine[20] = {0};
    char humiLine[20] = {0};

    (void)arg;

    if (SHT20_Init() != 0) {
        printf("SHT20 init failed.\r\n");
        return;
    }

    if (SSD1306_Init() != 0) {
        printf("SSD1306 init failed.\r\n");
        return;
    }

    SSD1306_CLS();
    SSD1306_ShowStr(0, 0, (uint8_t *)"SHT20", DISPLAY_TEXT_SIZE);

    while (1) {
        osSemaphoreAcquire(g_sampleSem, osWaitForever);

        if (SHT20_ReadData(&temperature, &humidity) != 0) {
            printf("SHT20 read failed.\r\n");
            SSD1306_ShowStr(0, 2, (uint8_t *)"Read failed", DISPLAY_TEXT_SIZE);
            continue;
        }

        (void)memset(tempLine, 0, sizeof(tempLine));
        (void)memset(humiLine, 0, sizeof(humiLine));
        (void)sprintf(tempLine, "Temp: %.2f C", temperature);
        (void)sprintf(humiLine, "Humi: %.2f%%", humidity);

        printf("temperature = %.2f C, humidity = %.2f%%RH\r\n", temperature, humidity);

        SSD1306_CLS();
        SSD1306_ShowStr(0, 0, (uint8_t *)"SHT20", DISPLAY_TEXT_SIZE);
        SSD1306_ShowStr(0, 2, (uint8_t *)tempLine, DISPLAY_TEXT_SIZE);
        SSD1306_ShowStr(0, 4, (uint8_t *)humiLine, DISPLAY_TEXT_SIZE);
    }
}

static void CreateSht20Thread(const char *name, osThreadFunc_t func)
{
    osThreadAttr_t attr;

    attr.name = name;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = SHT20_TASK_STACK_SIZE;
    attr.priority = SHT20_TASK_PRIO;

    if (osThreadNew(func, NULL, &attr) == NULL) {
        printf("Failed to create %s.\r\n", name);
    }
}

static void Sht20DemoEntry(void)
{
    g_sampleSem = osSemaphoreNew(4, 0, NULL);
    if (g_sampleSem == NULL) {
        printf("Failed to create SHT20 semaphore.\r\n");
        return;
    }

    printf("SHT20 OLED demo start.\r\n");
    CreateSht20Thread("Sht20Signal", SampleSignalTask);
    CreateSht20Thread("Sht20Display", Sht20DisplayTask);
}

APP_FEATURE_INIT(Sht20DemoEntry);
