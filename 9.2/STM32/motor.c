#include "motor.h"

/* L9110S motor driver pins from the schematic:
 * left motor:  L-IA PB7  -> TIM4_CH2 PWM, L-IB PB14 -> direction
 * right motor: R-IB PB6  -> TIM4_CH1 PWM, R-IA PB13 -> direction
 */
#define AIN     PBout(14)
#define BIN     PBout(13)
#define PWMA    TIM4->CCR2
#define PWMB    TIM4->CCR1
#define MOTOR_SPEED_SCALE 20

static u16 pwm_period = PWM_MAX;

static u32 myabs(long int value)
{
    return (value < 0) ? (u32)(-value) : (u32)value;
}

static u16 limit_pwm(u32 value)
{
    if (value > pwm_period) {
        return pwm_period;
    }

    return (u16)value;
}

static void Motor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13 | GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    AIN = 0;
    BIN = 0;
}

void PWM_Init(u16 arr, u16 psc)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    pwm_period = arr;

    Motor_Init();

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
    u16 left_pwm = limit_pwm(myabs(left_motor));
    u16 right_pwm = limit_pwm(myabs(right_motor));

    if (left_motor >= 0) {
        AIN = 0;
        PWMA = left_pwm;
    } else {
        AIN = 1;
        PWMA = pwm_period - left_pwm;
    }

    if (right_motor >= 0) {
        BIN = 0;
        PWMB = right_pwm;
    } else {
        BIN = 1;
        PWMB = pwm_period - right_pwm;
    }
}

void Motor_Stop(void)
{
    Set_Pwm(0, 0);
}

void stm32motor_control(int left_motor, int right_motor)
{
    Set_Pwm(left_motor * MOTOR_SPEED_SCALE, right_motor * MOTOR_SPEED_SCALE);
}

void car_stop(void)
{
    Motor_Stop();
}

void car_forward(void)
{
    stm32motor_control(100, 100);
}

void car_left(void)
{
    stm32motor_control(-50, 150);
}

void car_right(void)
{
    stm32motor_control(150, -50);
}

void car_backward(void)
{
    stm32motor_control(-150, -150);
}
