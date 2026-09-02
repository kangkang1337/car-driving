#include "stm32f10x.h"
#include "sys.h"

#define PWM_MAX 7199

/* L9110S pins from the schematic:
 * left motor:  L-IA PB7  -> TIM4_CH2 PWM, L-IB PB14 -> direction
 * right motor: R-IB PB6  -> TIM4_CH1 PWM, R-IA PB13 -> direction
 */
#define LEFT_DIR    PBout(14)
#define RIGHT_DIR   PBout(13)
#define LEFT_PWM    TIM4->CCR2
#define RIGHT_PWM   TIM4->CCR1

static u16 g_pwm_period = PWM_MAX;

static u32 myabs_long(long int value)
{
    return (value < 0) ? (u32)(-value) : (u32)value;
}

static u16 limit_pwm(u32 value)
{
    if (value > g_pwm_period) {
        return g_pwm_period;
    }

    return (u16)value;
}

static void Motor_Gpio_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* Direction pins: PB13, PB14. */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    LEFT_DIR = 0;
    RIGHT_DIR = 0;
}

void PWM_Init(u16 arr, u16 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    g_pwm_period = arr;

    Motor_Gpio_Init();

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* TIM4_CH1 -> PB6, TIM4_CH2 -> PB7. */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    TIM_TimeBaseStructure.TIM_Period = arr;
    TIM_TimeBaseStructure.TIM_Prescaler = psc;
    TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC2Init(TIM4, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_OC2PreloadConfig(TIM4, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM4, ENABLE);

    TIM_Cmd(TIM4, ENABLE);
}

void Set_Pwm(int left_motor, int right_motor)
{
    u16 left_pwm = limit_pwm(myabs_long(left_motor));
    u16 right_pwm = limit_pwm(myabs_long(right_motor));

    if (left_motor >= 0) {
        LEFT_DIR = 0;
        LEFT_PWM = left_pwm;
    } else {
        LEFT_DIR = 1;
        LEFT_PWM = g_pwm_period - left_pwm;
    }

    if (right_motor >= 0) {
        RIGHT_DIR = 0;
        RIGHT_PWM = right_pwm;
    } else {
        RIGHT_DIR = 1;
        RIGHT_PWM = g_pwm_period - right_pwm;
    }
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

    PWM_Init(PWM_MAX, 9);      /* 72 MHz / ((9 + 1) * (7199 + 1)) = 1 kHz. */
    colorful_led_Init();

    printf("QST PWM motor demo\r\n");

    while (1) {
        Set_Pwm(2500, 2500);
        delay_ms(1000);

        Set_Pwm(0, 0);
        delay_ms(500);

        Set_Pwm(-2500, -2500);
        delay_ms(1000);

        Set_Pwm(0, 0);
        delay_ms(500);
    }
}
