# car-driving
summer school from 8.24 to 9.6

# 8.26
# Hi3861 OpenHarmony Practice Notes

This folder contains several small OpenHarmony examples for the Hi3861 board.

## Projects

### 1.0Hello_world

This project contains serial output examples:

- `hello_world.c`: creates two CMSIS-RTOS2 threads. `thread1` prints `Hello World!` every second. `thread2` starts one second later and prints `Hello QST!` every three seconds.
- `square_loader.c`: prints a square-style loading animation every 100 ms.
- `progress_bar.c`: prints a progress bar with a spinner every 100 ms.
- `beijing_clock.c`: prints a UTC+8 software clock once per second. The clock starts from `08:00:00` after reset because no RTC or network time source is used.

Only one file should normally be selected in `BUILD.gn` at a time, because each file has its own `APP_FEATURE_INIT(...)` entry.

Example:

```gn
static_library("hello_world") {
  sources = [
    "progress_bar.c",
  ]

  include_dirs = [
    "//utils/native/lite/include",
    "//kernel/liteos_m/components/cmsis/2.0",
  ]
}
```

### 3.0SG90

This project controls an SG90 servo connected to GPIO2.

`SG90.c` generates a 20 ms servo control pulse:

- `500 us` high level: left side
- `1500 us` high level: middle position
- `2500 us` high level: right side

The current program creates one `ServoSweep` thread. It moves the servo smoothly from the left side to the right side, then smoothly back to the left side, and repeats forever.

Speed can be tuned with:

```c
#define SG90_DUTY_STEP_US 20
#define SG90_PULSE_REPEAT 2
```

Increase `SG90_DUTY_STEP_US` to move faster. Increase `SG90_PULSE_REPEAT` to move slower and hold each step longer.

## App BUILD.gn

The parent file `applications/sample/wifi-iot/app/BUILD.gn` must reference the real folder and target name.

Examples:

```gn
features = [
  "1.0_Hello_World:hello_world",
  "2.0_SG90:sg90",
]
```

The folder name, target name, and source file name must match exactly. Linux is case-sensitive, so `SG90.c` and `sg90.c` are different files.

## Build

Build in the OpenHarmony source tree:

```bash
python build.py wifiiot
```

If the build succeeds, flash the generated Hi3861 firmware with HiBurn. After flashing, close HiBurn or disconnect its serial connection, open the serial assistant, and press `RESET` to start the program.

## Common Issues

- `Unable to load ".../BUILD.gn"`: the parent `BUILD.gn` points to a folder that does not exist, or the folder name contains an unexpected character.
- `Unresolved dependencies`: the target name in the child `BUILD.gn` does not match the parent feature label.
- `fatal error: cmsis_os2.h: No such file or directory`: add the CMSIS include path in the child `BUILD.gn`.
- `undefined reference to fflush`: do not use `fflush(stdout)` on this Hi3861 LiteOS build.
- Chinese text appears as garbled characters in the serial assistant: switch the serial assistant to UTF-8, or print ASCII text only.
- HiBurn shows `Wait connect success flag (hisilicon) overtime`: close other serial tools, select the correct COM port, hold `BOOT`, click burn/connect, press `RESET`, then release `BOOT` after downloading starts. Also check that the board switch is set back to Hi3861.


# 8.27

## Overview

This Hi3861 workspace contains several OpenHarmony LiteOS sample projects for the QST smart car platform.

Today’s main work focused on the `4.0Hcsr` project, which uses the HC-SR04 ultrasonic sensor and system tick APIs.

## Project Path

```text
C:\Users\18500\Desktop\summer\SSH-192.168.13.128
```

Main sample folders:

```text
1.0Hello_world
3.0SG90
4.0Hcsr
```

## 4.0Hcsr Project

The `4.0Hcsr` project implements:

- HC-SR04 ultrasonic distance measurement
- CMSIS-RTOS2 software timers
- periodic system tick printing

Main files:

```text
4.0Hcsr/tick.c
4.0Hcsr/BUILD.gn
```

## Hardware Connection

According to the QST car schematic:

```text
TRIG -> GPIO7 / IO7
ECHO -> GPIO8 / IO8
VCC  -> 5V
GND  -> GND
```

The Hi3861 module uses 3.3V IO. Since many HC-SR04 modules output 5V on `ECHO`, a voltage divider or level shifter is recommended.

## Software Timers

Two CMSIS software timers are created in `tick.c`.

### Distance Measurement Timer

```c
#define HCSR04_MEASURE_PERIOD_TICKS 300
```

This timer triggers ultrasonic distance measurement about every 3 seconds.

### Tick Print Timer

```c
#define TICK_PRINT_PERIOD_TICKS 100
```

This timer prints the current system tick about every 1 second.

Example output:

```text
current tick: 143
current tick: 243
distance timeout: echo never high.
current tick: 346
```

## Distance Measurement Logic

The program sends a short trigger pulse through GPIO7:

```c
GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
hi_udelay(20);
GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);
```

Then it waits for GPIO8 to receive the ECHO pulse.

Distance is calculated from the ECHO high-level duration:

```c
distance = echo_time_us * 0.034 / 2;
```

The result is in centimeters.

## Timeout Messages

The program distinguishes two timeout cases.

```text
distance timeout: echo never high.
```

This means the ECHO pin never became high.

Possible causes:

- HC-SR04 has no power
- TRIG and ECHO are reversed
- ECHO is not connected to GPIO8
- GND is not shared
- sensor module is faulty
- ECHO voltage level is not compatible

```text
distance timeout: echo never low.
```

This means ECHO became high but never returned low.

Possible causes:

- ECHO pin is floating
- ECHO is stuck high
- wiring error
- sensor module abnormal behavior

## BUILD.gn

The `4.0Hcsr/BUILD.gn` file builds the project as a static library:

```gn
static_library("Hcsr04") {
  sources = [
    "tick.c",
  ]

  include_dirs = [
    "//utils/native/lite/include",
    "//kernel/liteos_m/components/cmsis/2.0",
    "//base/iot_hardware/interfaces/kits/wifiiot_lite",
  ]
}
```

To enable this sample in the OpenHarmony app build, the parent `BUILD.gn` should include:

```gn
"4.0Hcsr:Hcsr04",
```

## SG90 Servo Project

The `3.0SG90` project controls an SG90 servo.

Main file:

```text
3.0SG90/SG90.c
```

Servo signal pin:

```c
#define SG90_GPIO 2
```

Expected startup serial output:

```text
SG90 sweep start.
SG90 left to right.
SG90 right to left.
```

If the serial output appears but the servo does not move, check servo power and wiring. The servo should normally use an external 5V supply with common GND.

## Serial and Burning Notes

The QST car uses a CH340 USB-to-serial chip.

Important points:

- `COM3` appearing in Windows only proves that CH340 is detected.
- It does not always prove that the Hi3861 chip is running correctly.
- The USB serial switch must be set to the Hi3861 side.
- The board has a reset key, but no separate BOOT key.
- When using HiBurn, start burning first, then press the physical reset key when the tool waits for connection.

Common HiBurn error:

```text
Wait connect success flag (hisilicon) overtime.
```

This usually means HiBurn opened the COM port, but the Hi3861 did not enter the expected download handshake state.

Possible causes:

- wrong serial switch position
- unstable power
- reset timing issue
- Hi3861 not powered correctly
- wrong target firmware
- damaged or unstable board hardware

## Debug Summary

From today’s serial output:

```text
current tick: 143
current tick: 243
distance timeout: echo never high.
current tick: 346
```

The increasing tick value proves that:

- Hi3861 is running
- the task was created successfully
- both software timer logic and serial output are active

The repeated distance timeout indicates that the ultrasonic ECHO signal is not being detected. This points more toward sensor wiring, power, module condition, or voltage-level compatibility than toward the software timer logic.


# 8.28

Today I completed the UART, BLE, OLED, and SHT20 temperature-humidity sensor experiments on the Hi3861 development board.

## UART and BLE Communication

I implemented the UART communication project under `5.0Uart`. The program uses UART1 with GPIO0 as TXD and GPIO1 as RXD. It initializes the UART interface, receives data from the BLE module, and sends the received message back through UART.

The UART target name was changed to `UartDemo` to avoid a conflict with the system UART driver library. The parent `BUILD.gn` should use:

```gn
"5.0Uart:UartDemo",
```

During testing, I confirmed that the JDY-16 BLE module could connect successfully. The serial log showed:

```text
UART recv: +CONNECTED
```

After enabling `Subscribe` under `Notified Values` in the BLE debugging app, the phone was able to receive the echoed `HELLO` message. This confirmed that the BLE-to-UART communication path worked correctly.

## OLED Display

I completed the OLED SSD1306 project and added the required driver files from the support package. The project initializes the OLED through I2C and displays text and time information on the screen.

The OLED project includes the SSD1306 driver source and header files, and the `BUILD.gn` file was updated to include the correct source files and include paths.

## SHT20 Temperature and Humidity Project

I implemented the SHT20 project under `8.0SHT`. The project reads temperature and humidity data from the SHT20 sensor through I2C, then outputs the data in two ways:

- Displays temperature and humidity on the OLED screen
- Prints temperature and humidity values to UartAssist through `printf`

The project also uses a semaphore to control periodic sensor reading. The sensor data is refreshed every few seconds.

The parent `BUILD.gn` should include:

```gn
"8.0SHT:Sht20",
```

Since the SHT20 project already includes the OLED driver, the standalone OLED experiment should be disabled when compiling this project to avoid duplicate symbol errors.

## Result

By the end of the day, UART communication, BLE message receiving and echoing, OLED display, and SHT20 temperature-humidity output were all implemented and tested.
