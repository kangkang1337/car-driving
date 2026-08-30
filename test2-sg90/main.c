#include "stm32f10x.h"
#include "sys.h"
#include "usart.h"
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

    PWM_Init(PWM_MAX, 0);
    Motor_Stop();

    printf("QST motor UART receiver ready\r\n");

    while (1) {
        delay_ms(1000);
    }
}
