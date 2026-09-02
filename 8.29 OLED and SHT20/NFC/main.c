#include "stm32f10x.h"
#include "sys.h"
#include "nfc.h"
#include "motor.h"

int main(void)
{
    Stm32_Clock_Init(9);
    MY_NVIC_PriorityGroupConfig(2);
    uart_init(115200);
    JTAG_Set(JTAG_SWD_DISABLE);
    JTAG_Set(SWD_ENABLE);
    delay_init();
    PWM_Init(PWM_MAX, 9);
    printf("PWM init ok\r\n");
    colorful_led_Init();

    printf("QST NFC\r\n");
    NFC_Init();

    while (1) {
        NFC_Handler();
        delay_ms(20);
    }
}
