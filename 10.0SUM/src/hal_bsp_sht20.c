#include "hal_bsp_sht20.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_i2c.h"
#include "wifiiot_i2c_ex.h"

#define SHT20_NO_HOLD_TEMP_CMD 0xF3
#define SHT20_NO_HOLD_HUMI_CMD 0xF5
#define SHT20_SOFT_RESET_CMD 0xFE

static uint32_t SHT20_WriteByte(uint8_t byte)
{
    WifiIotI2cData i2cData = {0};

    i2cData.sendBuf = &byte;
    i2cData.sendLen = 1;

    return I2cWrite(SHT20_I2C_IDX, SHT20_I2C_ADDR, &i2cData);
}

static uint32_t SHT20_ReadBytes(uint8_t *data, size_t size)
{
    WifiIotI2cData i2cData = {0};

    i2cData.receiveBuf = data;
    i2cData.receiveLen = size;

    return I2cRead(SHT20_I2C_IDX, SHT20_I2C_ADDR, &i2cData);
}

uint32_t SHT20_ReadData(float *temp, float *humi)
{
    uint32_t result;
    uint8_t buffer[3] = {0};
    uint16_t raw;

    if (temp == NULL || humi == NULL) {
        return 1;
    }

    result = SHT20_WriteByte(SHT20_NO_HOLD_TEMP_CMD);
    if (result != 0) {
        printf("SHT20 temp command failed, ret=0x%x\r\n", result);
        return result;
    }
    usleep(85000);

    result = SHT20_ReadBytes(buffer, sizeof(buffer));
    if (result != 0) {
        printf("SHT20 temp read failed, ret=0x%x\r\n", result);
        return result;
    }

    raw = ((uint16_t)buffer[0] << 8) | (buffer[1] & 0xFC);
    *temp = 175.72f * ((float)raw / 65536.0f) - 46.85f;

    (void)memset(buffer, 0, sizeof(buffer));
    result = SHT20_WriteByte(SHT20_NO_HOLD_HUMI_CMD);
    if (result != 0) {
        printf("SHT20 humi command failed, ret=0x%x\r\n", result);
        return result;
    }
    usleep(50000);

    result = SHT20_ReadBytes(buffer, sizeof(buffer));
    if (result != 0) {
        printf("SHT20 humi read failed, ret=0x%x\r\n", result);
        return result;
    }

    raw = ((uint16_t)buffer[0] << 8) | (buffer[1] & 0xFC);
    *humi = 125.0f * ((float)raw / 65536.0f) - 6.0f;

    return 0;
}

uint32_t SHT20_Init(void)
{
    uint32_t result;

    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_10, WIFI_IOT_IO_FUNC_GPIO_10_I2C0_SDA);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_9, WIFI_IOT_IO_FUNC_GPIO_9_I2C0_SCL);
    (void)I2cInit(SHT20_I2C_IDX, SHT20_I2C_SPEED);
    (void)I2cSetBaudrate(SHT20_I2C_IDX, SHT20_I2C_SPEED);

    result = SHT20_WriteByte(SHT20_SOFT_RESET_CMD);
    if (result != 0) {
        printf("SHT20 reset failed, ret=0x%x\r\n", result);
        return result;
    }

    usleep(100000);
    return 0;
}
