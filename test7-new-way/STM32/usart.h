#ifndef __USART_H
#define __USART_H

#include "stdio.h"
#include "sys.h"

#define USART_REC_LEN 200
#define EN_USART1_RX 1

extern u8 USART_RX_BUF[USART_REC_LEN];
extern u8 USART_RX_STA;
extern u8 LED_CMD;
extern u8 TURN_CMD;
extern u8 END_CMD;

void uart_init(u32 bound);

#endif
