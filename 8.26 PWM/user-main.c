#include "stm32f10x.h"
#include "sys.h"
#include "motor.h"

#define START_PWM         3300
#define BASE_PWM          3000
#define FAST_PWM          3400
#define SLOW_PWM          2600
#define SWING_FAST_PWM    3300
#define SWING_SLOW_PWM    2600

static void Check_Stop_Command(void)
{
    if (USART_RX_STA == 1) {
        USART_RX_STA = 0;
        Motor_Stop();
        printf("Stop 10s\r\n");
        delay_ms(10000);
    }
}

static void Run_Motor_For(int left_motor, int right_motor, u16 time_ms)
{
    u16 elapsed = 0;

    Set_Pwm(left_motor, right_motor);

    while (elapsed < time_ms) {
        Check_Stop_Command();
        delay_ms(20);
        elapsed += 20;
    }
}

static void Start_Boost(void)
{
    Run_Motor_For(START_PWM, START_PWM, 300);
}

static void Swing_Head(u8 times)
{
    u8 i;

    for (i = 0; i < times; i++) {
        Run_Motor_For(SWING_FAST_PWM, SWING_SLOW_PWM, 450);
        Run_Motor_For(SWING_SLOW_PWM, SWING_FAST_PWM, 450);
    }
}

static void Closed_Curve_With_Swing(void)
{
    Start_Boost();
    Run_Motor_For(FAST_PWM, SLOW_PWM, 2400);
    Swing_Head(3);

    Run_Motor_For(BASE_PWM, BASE_PWM, 1000);

    Run_Motor_For(SLOW_PWM, FAST_PWM, 4800);
    Swing_Head(3);

    Run_Motor_For(BASE_PWM, BASE_PWM, 1000);
    Run_Motor_For(FAST_PWM, SLOW_PWM, 2400);
}

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
    colorful_led_Init();

    printf("QST closed curve with boost\r\n");

    while (1) {
        Closed_Curve_With_Swing();
    }
}
