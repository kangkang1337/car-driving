#include "stm32f10x.h"
#include "sys.h"
#include "encoder.h"
#include "control_system.h"
#include "motor.h"

int main(void)
{
    RCC->CSR |= 1 << 24;
    Stm32_Clock_Init(9);
    MY_NVIC_PriorityGroupConfig(2);
    uart_init(115200);
    JTAG_Set(JTAG_SWD_DISABLE);
    JTAG_Set(SWD_ENABLE);
    delay_init();

    Encoder_Init_TIM2();
    Encoder_Init_TIM3();
    PWM_Init(PWM_MAX, 0);
    colorful_led_Init();

    SysTick_Config(72000000 / 1000);

    printf("QST PID motor speed close loop\r\n");

    while (1) {
        delay_ms(100);
    }
}
