#include "nfc.h"

const u8 NFC_WakeUp[] = {
    0x55, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x03, 0xFD,
    0xD4, 0x14, 0x01, 0x17, 0x00
};

const u8 NFC_SearchCard[] = {
    0x00, 0x00, 0xFF, 0x04, 0xFC, 0xD4,
    0x4A, 0x01, 0x00, 0xE1, 0x00
};

u8 NFC_WakeUp_Ok = 0;
u8 NFC_find_Card = 0;
u8 NFC_sendcmd_find = 1;
u8 NFC_read_id_flag = 0;
u8 NFC_DataBlock[16];
u8 USART2_RX_BUF[USART2_REC_LEN];
u16 slen = 0;
u8 Sys_Stat = 0;
u8 Sum = 0;
u8 REC_LEN = 0;
u8 led_flag = 0;

static u8 NFC_IsKnownCard(u8 *buf, u16 len)
{
    if (len < 23) {
        return 0;
    }

    if ((buf[19] == 0xB9) && (buf[20] == 0x80) &&
        (buf[21] == 0x06) && (buf[22] == 0x85)) {
        return 1;
    }

    if ((buf[19] == 0x50) && (buf[20] == 0x84) &&
        (buf[21] == 0xFC) && (buf[22] == 0x23)) {
        return 1;
    }

    if ((buf[19] == 0x40) && (buf[20] == 0x74) &&
        (buf[21] == 0x80) && (buf[22] == 0x23)) {
        return 1;
    }

    return 0;
}

void NFC_Init(void)
{
    UART2_Init(115200);
    UART2SendFrame((u8 *)NFC_WakeUp, sizeof(NFC_WakeUp));
}

void NFC_Handler(void)
{
    if (NFC_WakeUp_Ok) {
        if (NFC_find_Card == 1) {
            FoundCard_Handler();
        } else if ((NFC_find_Card == 0) && (NFC_sendcmd_find == 1)) {
            UART2Frame.RxCounter = 0;
            UART2SendFrame((u8 *)NFC_SearchCard, sizeof(NFC_SearchCard));
            NFC_sendcmd_find = 0;
            delay_ms(200);
        }
    }
}

void NFC_user_Handler(void)
{
    NFC_Handler();
}

void NFC_ProcessRxFrame(u8 *buf, u16 len)
{
    if (NFC_WakeUp_Ok == 0) {
        if ((len >= 15) && (buf[11] == 0xD5) && (buf[12] == 0x15)) {
            NFC_WakeUp_Ok = 1;
            NFC_sendcmd_find = 1;
            printf("NFC wake up ok\r\n");
        }

        return;
    }

    put_HEX(USART1, buf, len);

    if (NFC_IsKnownCard(buf, len)) {
        NFC_find_Card = 1;
    } else {
        NFC_sendcmd_find = 1;
    }
}

void FoundCard_Handler(void)
{
    NFC_find_Card = 0;

    if (led_flag == 0) {
        led_flag = 1;
        R_led_mode();
    } else {
        led_flag = 0;
        R_led_CLC();
    }

    NFC_sendcmd_find = 1;
    delay_ms(200);
}
