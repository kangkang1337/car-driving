#ifndef __NFC_H
#define __NFC_H

#include "sys.h"

extern const u8 NFC_WakeUp[];
extern const u8 NFC_SearchCard[];
extern u8 NFC_WakeUp_Ok;
extern u8 NFC_find_Card;
extern u8 NFC_sendcmd_find;
extern u8 NFC_read_id_flag;
extern u8 USART2_RX_BUF[USART2_REC_LEN];

void NFC_Init(void);
void NFC_Handler(void);
void NFC_user_Handler(void);
void NFC_ProcessRxFrame(u8 *buf, u16 len);
void FoundCard_Handler(void);

#endif
