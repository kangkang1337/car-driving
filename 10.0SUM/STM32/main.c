#include "stm32f10x.h"
#include "sys.h"
#include "colorful_led.h"
#include "usart.h"
#include "motor.h"

static void CarLightSet(u8 on)
{
    u8 i;

    for (i = 1; i <= led_num; i++) {
        if (on != 0) {
            L_ws2812_rgb(i, WS_WHITE);
            R_ws2812_rgb(i, WS_WHITE);
        } else {
            L_ws2812_rgb(i, WS_DARK);
            R_ws2812_rgb(i, WS_DARK);
        }
    }

    L_ws2812_refresh(led_num);
    R_ws2812_refresh(led_num);
}

int main(void)
{
    u8 currentLed = 0;

    RCC->CSR |= 1 << 24;
    Stm32_Clock_Init(9);
    MY_NVIC_PriorityGroupConfig(2);
    uart_init(115200);
    JTAG_Set(JTAG_SWD_DISABLE);
    JTAG_Set(SWD_ENABLE);
    delay_init();

    PWM_Init(PWM_MAX, 0);
    Motor_Stop();
    colorful_led_Init();
    CarLightSet(0);

    SysTick_Config(72000000 / 1000);

    printf("QST motor and LED UART receiver ready\r\n");

    while (1) {
        if (USART_RX_STA != 0) {
            USART_RX_STA = 0;
            if (currentLed != LED_CMD) {
                currentLed = LED_CMD;
                CarLightSet(currentLed);
            }
        }

        delay_ms(20);
    }
}
