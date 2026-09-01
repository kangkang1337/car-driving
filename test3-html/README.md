# 10.0SUM smart car integration

Run-time startup text must be:

```text
10.0SUM project start.
Task 1 running: car safety: IR edge protection + ultrasonic obstacle avoidance
Task 2 running: OLED display: SHT20 temperature/humidity + AP3216C IR/ALS/PS
Task 3 running: WiFi connect
Task 4 running: Huawei Cloud MQTT realtime upload
```

If the serial port still prints `AP3216C ir=..., als=..., ps=..., led=...`, the board is still running the old `9.0AP3216` firmware, not this project.

## Hi3861 build target

Use this directory's target:

```text
10.0SUM:sum
```

Do not build or burn `9.0AP3216:Ap3216c` for the final car project.

## STM32 side

Burn `TIMER/USER/main.c` from this project after rebuilding the Keil project. The STM32 program waits for 6-byte motor frames from Hi3861 at 115200 baud:

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

The previous fixed-PWM test code has been removed from the main entry.

The same UART also accepts the light commands used by `9.0AP3216`:

```text
L1: LED on
L0: LED off
YL: left side yellow turn signal
YR: right side yellow turn signal
Y0: turn signal off
```

## WiFi

Edit the WiFi credentials in `sum.c` before burning the Hi3861 firmware:

```c
#define WIFI_SSID "iPhone"
#define WIFI_PSK "nihao1009"
```

Task 3 scans for the configured SSID, connects in station mode, and starts DHCP. If the hotspot is not found or DHCP times out, Task 1 car safety and Task 2 OLED display continue running.

## Huawei Cloud IoTDA

Edit the Huawei Cloud MQTT parameters in `sum.c` before burning the Hi3861 firmware:

```c
#define HUAWEI_IOT_HOST "3c95083845.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#define HUAWEI_IOT_PORT 1883
#define HUAWEI_DEVICE_ID "6a9643457f2e6c302f94fcf9_qstcar"
#define HUAWEI_CLIENT_ID "6a9643457f2e6c302f94fcf9_qstcar_0_0_2026090104"
#define HUAWEI_USERNAME "6a9643457f2e6c302f94fcf9_qstcar"
#define HUAWEI_PASSWORD "b9fc45c0f75d988c8bf279c96d84e6db2c791e47c29b58e6ee42184a467d6134"
```

Task 4 waits until Task 3 has connected WiFi, then connects to Huawei Cloud IoTDA by MQTT and reports car data every 2 seconds.
`sum.c` uses `DelayMs()` because CMSIS `osDelay()` takes OS ticks on this board; with a 10 ms tick, calling `osDelay(5000)` directly would delay about 50 seconds.

The IoTDA product model should contain service `qstcar`. Reported properties:

```text
temp
humi
lumi
mode_led
car_mode
temperature
humidity
ap_ir
ap_als
ap_ps
edge_left
edge_right
distance_cm
led_on
```

A matching product-model reference file is included at `profile/devicetype-capability.json`.

## Wiring used by Hi3861

- GPIO13: left infrared table-edge sensor
- GPIO14: right infrared table-edge sensor
- Infrared edge level is configured by `IR_EDGE_VALUE` in `sum.c`; current default is `HI_GPIO_VALUE1`.
- GPIO7: HC-SR04 Trig
- GPIO8: HC-SR04 Echo
- GPIO2: SG90 signal
- GPIO11/GPIO12: UART2 TX/RX to STM32, 115200 baud
- Bluetooth UART1 is not used; Task 3 is WiFi.
- GPIO9/GPIO10: I2C0 SCL/SDA for OLED, SHT20 and AP3216C
- GPIO6: LED, same as `9.0AP3216`: high is off, ALS <= 50 drives low/on
