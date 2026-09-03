#include "stm32f10x.h"
#include "sys.h"
#include "colorful_led.h"
#include "usart.h"
#include "motor.h"

static void CarLightSet(u8 on, u8 turn)
{
    u8 i;

    for (i = 1; i <= led_num; i++) {
        if (turn == 1) {
            if (i >= 4) {
                L_ws2812_rgb(i, WS_YELLOW);
            } else {
                L_ws2812_rgb(i, WS_DARK);
            }
            R_ws2812_rgb(i, WS_DARK);
        } else if (turn == 2) {
            if (i <= 3) {
                L_ws2812_rgb(i, WS_YELLOW);
            } else {
                L_ws2812_rgb(i, WS_DARK);
            }
            R_ws2812_rgb(i, WS_DARK);
        } else if (on != 0) {
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

static void CarLightBlinkAll(void)
{
    CarLightSet(1, 0);
    delay_ms(250);
    CarLightSet(0, 0);
    delay_ms(250);
}

int main(void)
{
    u8 currentLed = 0;
    u8 currentTurn = 0;

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
    CarLightSet(0, 0);

    SysTick_Config(72000000 / 1000);

    printf("QST motor and LED UART receiver ready\r\n");

    while (1) {
        if (END_CMD != 0) {
            Motor_Stop();
            CarLightBlinkAll();
            continue;
        }

        if (USART_RX_STA != 0) {
            USART_RX_STA = 0;
            if (currentLed != LED_CMD || currentTurn != TURN_CMD) {
                currentLed = LED_CMD;
                currentTurn = TURN_CMD;
                CarLightSet(currentLed, currentTurn);
            }
        }

        delay_ms(20);
    }
}
