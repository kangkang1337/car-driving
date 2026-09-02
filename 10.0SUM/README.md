# 10.0SUM smart car integration

Run-time startup text must be:

```text
10.0SUM project start.
Task 1 running: car safety: IR edge protection + ultrasonic obstacle avoidance
Task 2 running: OLED display: SHT20 temperature/humidity + AP3216C IR/ALS/PS
Task 3 running: Bluetooth UART1 communication
```

If the serial port still prints `AP3216C ir=..., als=..., ps=..., led=...`, the board is still running the old `9.0AP3216` firmware, not this project.

## Hi3861 build target

Use this directory's target:

```text
10.0SUM:SumCar
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
```

## Wiring used by Hi3861

- GPIO14: left infrared table-edge sensor
- GPIO13: right infrared table-edge sensor
- Infrared edge level is configured by `IR_EDGE_VALUE` in `sum.c`; current default is `HI_GPIO_VALUE1`.
- GPIO7: HC-SR04 Trig
- GPIO8: HC-SR04 Echo
- GPIO2: SG90 signal
- GPIO11/GPIO12: UART2 TX/RX to STM32, 115200 baud
- GPIO0/GPIO1: Bluetooth UART1 TX/RX, 9600 baud
- GPIO9/GPIO10: I2C0 SCL/SDA for OLED, SHT20 and AP3216C
- GPIO6: LED, same as `9.0AP3216`: high is off, ALS <= 50 drives low/on
