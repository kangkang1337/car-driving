#include "sys.h"
#include "usart.h"
#include "motor.h"

#if 1
#pragma import(__use_no_semihosting)
struct __FILE
{
    int handle;
};

FILE __stdout;

_sys_exit(int x)
{
    x = x;
}

int fputc(int ch, FILE *f)
{
    while ((USART1->SR & 0X40) == 0);
    USART1->DR = (u8)ch;
    return ch;
}
#endif

#if EN_USART1_RX

u8 USART_RX_BUF[USART_REC_LEN];
u8 USART_RX_STA = 0;
u8 LED_CMD = 0;
u8 TURN_CMD = 0;
u8 END_CMD = 0;
static u8 motor_frame[6];
static u8 motor_frame_index = 0;
static u8 led_frame_started = 0;
static u8 turn_frame_started = 0;
static u8 end_frame_started = 0;

void uart_init(u32 bound)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1 | RCC_APB2Periph_GPIOA, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_InitStructure.USART_BaudRate = bound;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;

    USART_Init(USART1, &USART_InitStructure);
    USART_ITConfig(USART1, USART_IT_RXNE, ENABLE);
    USART_Cmd(USART1, ENABLE);
}

static void ParseLedByte(u8 res)
{
    if (res == 'L') {
        led_frame_started = 1;
        motor_frame_index = 0;
        return;
    }

    if (led_frame_started == 0) {
        return;
    }

    if (res == '1') {
        LED_CMD = 1;
        USART_RX_STA = 1;
    } else if (res == '0') {
        LED_CMD = 0;
        USART_RX_STA = 1;
    }

    led_frame_started = 0;
}

static void ParseTurnByte(u8 res)
{
    if (res == 'Y') {
        turn_frame_started = 1;
        motor_frame_index = 0;
        return;
    }

    if (turn_frame_started == 0) {
        return;
    }

    if (res == 'L') {
        TURN_CMD = 1;
        USART_RX_STA = 1;
    } else if (res == 'R') {
        TURN_CMD = 2;
        USART_RX_STA = 1;
    } else if (res == '0') {
        TURN_CMD = 0;
        USART_RX_STA = 1;
    }

    turn_frame_started = 0;
}

static void ParseEndByte(u8 res)
{
    if (res == 'E') {
        end_frame_started = 1;
        motor_frame_index = 0;
        return;
    }

    if (end_frame_started == 0) {
        return;
    }

    END_CMD = 1;
    USART_RX_STA = 1;
    end_frame_started = 0;
}

static void ParseMotorByte(u8 res)
{
    if (motor_frame_index == 0 && res != 0xFC) {
        return;
    }

    motor_frame[motor_frame_index++] = res;
    if (motor_frame_index >= sizeof(motor_frame)) {
        if (motor_frame[0] == 0xFC && motor_frame[5] == 0xFD) {
            int left_speed = motor_frame[2];
            int right_speed = motor_frame[4];

            if (motor_frame[1] != 0) {
                left_speed = -left_speed;
            }
            if (motor_frame[3] != 0) {
                right_speed = -right_speed;
            }

            Set_Pwm(left_speed * 20, right_speed * 20);
            END_CMD = 0;
        }

        motor_frame_index = 0;
    }
}

void USART1_IRQHandler(void)
{
    u8 res;

    if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET) {
        res = USART_ReceiveData(USART1);

        if (motor_frame_index > 0) {
            ParseMotorByte(res);
        } else if (res == 'E' || end_frame_started != 0) {
            ParseEndByte(res);
        } else if (res == 'Y' || turn_frame_started != 0) {
            ParseTurnByte(res);
        } else if (res == 'L' || led_frame_started != 0) {
            ParseLedByte(res);
        } else {
            ParseMotorByte(res);
        }
    }

    USART_ClearFlag(USART1, USART_FLAG_RXNE);
}

#endif
