#ifndef HAL_BSP_SHT20_H
#define HAL_BSP_SHT20_H

#include <stdint.h>

#define SHT20_I2C_ADDR 0x80
#define SHT20_I2C_IDX  0
#define SHT20_I2C_SPEED 400000

uint32_t SHT20_Init(void);
uint32_t SHT20_ReadData(float *temp, float *humi);

#endif
