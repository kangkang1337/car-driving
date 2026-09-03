#include "hal_bsp_ap3216c.h"

#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_i2c_ex.h"

#define AP3216C_SYSTEM_ADDR 0x00
#define AP3216C_IR_L_ADDR   0x0A
#define AP3216C_IR_H_ADDR   0x0B
#define AP3216C_ALS_L_ADDR  0x0C
#define AP3216C_ALS_H_ADDR  0x0D
#define AP3216C_PS_L_ADDR   0x0E
#define AP3216C_PS_H_ADDR   0x0F

static uint16_t g_ap3216cAddr = AP3216C_I2C_ADDR;

void AP3216C_I2cBusInit(void)
{
    GpioInit();
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_10, WIFI_IOT_IO_FUNC_GPIO_10_I2C0_SDA);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_9, WIFI_IOT_IO_FUNC_GPIO_9_I2C0_SCL);
    (void)I2cInit(AP3216C_I2C_IDX, AP3216C_I2C_SPEED);
    (void)I2cSetBaudrate(AP3216C_I2C_IDX, AP3216C_I2C_SPEED);
}

static uint32_t AP3216C_WriteByte(uint8_t byte)
{
    WifiIotI2cData i2cData = {0};

    i2cData.sendBuf = &byte;
    i2cData.sendLen = 1;

    return I2cWrite(AP3216C_I2C_IDX, g_ap3216cAddr, &i2cData);
}

static uint32_t AP3216C_ReadBytes(uint8_t *data, size_t size)
{
    WifiIotI2cData i2cData = {0};

    i2cData.receiveBuf = data;
    i2cData.receiveLen = size;

    return I2cRead(AP3216C_I2C_IDX, g_ap3216cAddr, &i2cData);
}

static uint32_t AP3216C_WriteReg(uint8_t regAddr, uint8_t byte)
{
    uint8_t buffer[] = {regAddr, byte};
    WifiIotI2cData i2cData = {0};

    i2cData.sendBuf = buffer;
    i2cData.sendLen = sizeof(buffer);

    return I2cWrite(AP3216C_I2C_IDX, g_ap3216cAddr, &i2cData);
}

static uint32_t AP3216C_ReadReg(uint8_t regAddr, uint8_t *byte)
{
    uint32_t result;
    uint8_t buffer = 0;

    if (byte == NULL) {
        return 1;
    }

    result = AP3216C_WriteByte(regAddr);
    if (result != 0) {
        return result;
    }

    result = AP3216C_ReadBytes(&buffer, sizeof(buffer));
    if (result != 0) {
        return result;
    }

    *byte = buffer;
    return 0;
}

uint32_t AP3216C_ReadData(uint16_t *irData, uint16_t *alsData, uint16_t *psData)
{
    uint32_t result;
    uint8_t dataL = 0;
    uint8_t dataH = 0;

    if (irData == NULL || alsData == NULL || psData == NULL) {
        return 1;
    }

    result = AP3216C_ReadReg(AP3216C_IR_L_ADDR, &dataL);
    if (result != 0) {
        return result;
    }
    result = AP3216C_ReadReg(AP3216C_IR_H_ADDR, &dataH);
    if (result != 0) {
        return result;
    }
    *irData = (dataL & 0x80) ? 0 : (((uint16_t)dataH << 2) | (dataL & 0x03));

    result = AP3216C_ReadReg(AP3216C_ALS_L_ADDR, &dataL);
    if (result != 0) {
        return result;
    }
    result = AP3216C_ReadReg(AP3216C_ALS_H_ADDR, &dataH);
    if (result != 0) {
        return result;
    }
    *alsData = ((uint16_t)dataH << 8) | dataL;

    result = AP3216C_ReadReg(AP3216C_PS_L_ADDR, &dataL);
    if (result != 0) {
        return result;
    }
    result = AP3216C_ReadReg(AP3216C_PS_H_ADDR, &dataH);
    if (result != 0) {
        return result;
    }
    *psData = (dataL & 0x40) ? 0 : (((uint16_t)(dataH & 0x3F) << 4) | (dataL & 0x0F));

    return 0;
}

uint32_t AP3216C_Init(void)
{
    uint32_t result;
    unsigned int retry;

    AP3216C_I2cBusInit();
    g_ap3216cAddr = AP3216C_I2C_ADDR;

    for (retry = 0; retry < 3; retry++) {
        result = AP3216C_WriteReg(AP3216C_SYSTEM_ADDR, 0x04);
        if (result == 0) {
            break;
        }
        printf("AP3216C reset retry %u failed, addr=0x%x ret=0x%x\r\n",
            retry + 1, g_ap3216cAddr, result);
        usleep(100000);
    }

    if (result != 0) {
        printf("AP3216C no response at 0x3c. OLED I2C works, check AP3216C module wiring/power.\r\n");
        return result;
    }
    usleep(5000);

    result = AP3216C_WriteReg(AP3216C_SYSTEM_ADDR, 0x03);
    if (result != 0) {
        printf("AP3216C enable failed, ret=0x%x\r\n", result);
        return result;
    }

    usleep(150000);
    return 0;
}
