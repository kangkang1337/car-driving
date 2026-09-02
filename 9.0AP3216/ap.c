#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_ssd1306.h"
#include "ohos_init.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"

#define AP3216_TASK_STACK_SIZE (1024 * 4)
#define AP3216_TASK_PRIO 25
#define OLED_TEXT_SIZE 16
#define CAR_LED_GPIO WIFI_IOT_IO_NAME_GPIO_6
#define CAR_LED_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_6_GPIO
#define ALS_DARK_THRESHOLD 50
#define STM32_UART WIFI_IOT_UART_IDX_2
#define STM32_UART_BAUD 9600

static unsigned char g_stm32UartReady = 0;

static void CarLedInit(void)
{
    GpioInit();
    IoSetFunc(CAR_LED_GPIO, CAR_LED_IO_FUNC);
    GpioSetDir(CAR_LED_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(CAR_LED_GPIO, WIFI_IOT_GPIO_VALUE1);
}

static void CarLedSetByLight(uint16_t als)
{
    WifiIotGpioValue value = (als <= ALS_DARK_THRESHOLD) ? WIFI_IOT_GPIO_VALUE0 : WIFI_IOT_GPIO_VALUE1;
    GpioSetOutputVal(CAR_LED_GPIO, value);
}

static void Stm32UartInit(void)
{
    WifiIotUartAttribute uartAttr = {
        .baudRate = STM32_UART_BAUD,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);

    if (UartInit(STM32_UART, &uartAttr, NULL) == 0) {
        g_stm32UartReady = 1;
        printf("STM32 UART2 ready.\r\n");
    } else {
        printf("STM32 UART2 init failed.\r\n");
    }
}

static void Stm32LedSetByLight(uint16_t als)
{
    const unsigned char *cmd = (als <= ALS_DARK_THRESHOLD) ? (const unsigned char *)"L1\n" : (const unsigned char *)"L0\n";

    if (g_stm32UartReady != 0) {
        (void)UartWrite(STM32_UART, cmd, 3);
    }
}

static void AP3216DisplayTask(void *arg)
{
    uint16_t ir = 0;
    uint16_t als = 0;
    uint16_t ps = 0;
    char alsLine[20] = {0};
    char irLine[20] = {0};
    char ledLine[20] = {0};

    (void)arg;

    CarLedInit();
    AP3216C_I2cBusInit();

    if (SSD1306_Init() != 0) {
        printf("SSD1306 init failed.\r\n");
        return;
    }

    if (AP3216C_Init() != 0) {
        printf("AP3216C init failed.\r\n");
        SSD1306_CLS();
        SSD1306_ShowStr(0, 0, (uint8_t *)"AP3216C", OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 1, (uint8_t *)"Init failed", OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 2, (uint8_t *)"Check I2C", OLED_TEXT_SIZE);
        return;
    }

    Stm32UartInit();

    printf("AP3216C light demo start.\r\n");
    SSD1306_CLS();
    SSD1306_ShowStr(0, 0, (uint8_t *)"AP3216C", OLED_TEXT_SIZE);

    while (1) {
        if (AP3216C_ReadData(&ir, &als, &ps) != 0) {
            printf("AP3216C read failed.\r\n");
            SSD1306_CLS();
            SSD1306_ShowStr(0, 0, (uint8_t *)"AP3216C", OLED_TEXT_SIZE);
            SSD1306_ShowStr(0, 1, (uint8_t *)"Read failed", OLED_TEXT_SIZE);
            sleep(1);
            continue;
        }

        CarLedSetByLight(als);
        Stm32LedSetByLight(als);

        (void)memset(alsLine, 0, sizeof(alsLine));
        (void)memset(irLine, 0, sizeof(irLine));
        (void)memset(ledLine, 0, sizeof(ledLine));
        (void)snprintf(alsLine, sizeof(alsLine), "ALS:%u", (unsigned int)als);
        (void)snprintf(irLine, sizeof(irLine), "IR:%u PS:%u", (unsigned int)ir, (unsigned int)ps);
        (void)snprintf(ledLine, sizeof(ledLine), "LED:%s", (als <= ALS_DARK_THRESHOLD) ? "ON" : "OFF");

        printf("AP3216C ir=%u, als=%u, ps=%u, led=%s\r\n",
            (unsigned int)ir, (unsigned int)als, (unsigned int)ps,
            (als <= ALS_DARK_THRESHOLD) ? "ON" : "OFF");

        SSD1306_CLS();
        SSD1306_ShowStr(0, 0, (uint8_t *)"AP3216C", OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 1, (uint8_t *)alsLine, OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 2, (uint8_t *)irLine, OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 3, (uint8_t *)ledLine, OLED_TEXT_SIZE);

        sleep(1);
    }
}

static void AP3216DemoEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "AP3216Demo";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = AP3216_TASK_STACK_SIZE;
    attr.priority = AP3216_TASK_PRIO;

    if (osThreadNew(AP3216DisplayTask, NULL, &attr) == NULL) {
        printf("Failed to create AP3216Demo task.\r\n");
    }
}

APP_FEATURE_INIT(AP3216DemoEntry);
