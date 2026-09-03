#ifndef HAL_BSP_AP3216C_H
#define HAL_BSP_AP3216C_H

#include <stdint.h>
#include "cmsis_os2.h"
#include "wifiiot_i2c.h"

#define AP3216C_I2C_ADDR  (0x3C)
#define AP3216C_I2C_IDX   WIFI_IOT_I2C_IDX_0
#define AP3216C_I2C_SPEED 400000

uint32_t AP3216C_Init(void);
void AP3216C_I2cBusInit(void);
uint32_t AP3216C_ReadData(uint16_t *irData, uint16_t *alsData, uint16_t *psData);

#endif
