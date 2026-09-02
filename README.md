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

# 8.31

## AP3216C Light LED Demo

Implemented an AP3216C-based light control demo under:

```text
C:\Users\18500\Desktop\summer\SSH-192.168.13.128\9.0AP3216
```

## Features

- Reads ambient light data from the AP3216C sensor through I2C.
- Prints sensor data to the serial terminal.
- Displays sensor data and LED state on the OLED screen.
- Uses ambient light intensity to control the car LEDs:
  - When the light is blocked or weak, the LEDs turn on in white.
  - When light is detected, the LEDs turn off.

## Hi3861 Side

Main project files:

```text
9.0AP3216\ap.c
9.0AP3216\include\hal_bsp_ap3216c.h
9.0AP3216\src\hal_bsp_ap3216c.c
9.0AP3216\src\hal_bsp_ssd1306.c
```

The Hi3861 reads AP3216C data and prints output such as:

```text
AP3216C ir=4, als=338, ps=151, led=OFF
AP3216C ir=1, als=6, ps=229, led=ON
```

The light threshold is:

```text
ALS <= 50: LED ON
ALS > 50: LED OFF
```

## OLED Display

The OLED displays:

```text
AP3216C
ALS:xxx
IR:xxx PS:xxx
LED:ON/OFF
```

## UART Control

The Hi3861 sends LED control commands to the STM32 through UART2:

```text
GPIO11: UART2 TX
GPIO12: UART2 RX
Baudrate: 9600
```

Commands:

```text
L1: turn on all car LEDs in white
L0: turn off all car LEDs
```

## STM32 Side

STM32 LED project path:

```text
9.0AP3216\led
```

Main files:

```text
9.0AP3216\led\USER\main.c
9.0AP3216\led\SYSTEM\usart\usart.c
9.0AP3216\led\SYSTEM\usart\usart.h
```

The STM32 receives UART commands from the Hi3861:

```text
L1 -> all left and right WS2812 LEDs turn white
L0 -> all left and right WS2812 LEDs turn off
```

Unrelated motor, encoder, and PWM logic was removed from `main.c`.

## Notes

During testing, AP3216C initialization failed with:

```text
AP3216C reset failed, status=0x80001182
```

The issue was caused by the hardware connection. Replugging the data cable restored normal AP3216C output.

## Project Summary

Integrated the final smart car project in `10.0SUM`.

The project now includes:

- Infrared pair table-edge detection to prevent the car from falling.
- HC-SR04 ultrasonic obstacle avoidance.
- SG90 servo scanning for left/right obstacle checking.
- Basic Bluetooth UART communication.
- OLED display for SHT20 temperature/humidity and AP3216C IR/ALS/PS data.
- Serial logs showing which task is running and what it is doing.
- STM32 motor control through UART frames.
- STM32 LED control compatible with the previous `9.0AP3216` LED behavior.

## Main Files

Hi3861 side:

- `10.0SUM/sum.c`
- `10.0SUM/BUILD.gn`
- `10.0SUM/include/hal_bsp_sht20.h`
- `10.0SUM/src/hal_bsp_sht20.c`
- `10.0SUM/src/hal_bsp_ap3216c.c`
- `10.0SUM/src/hal_bsp_ssd1306.c`

STM32 side:

- `10.0SUM/TIMER/USER/main.c`
- `10.0SUM/TIMER/SYSTEM/usart/usart.c`
- `10.0SUM/TIMER/SYSTEM/usart/usart.h`

## Runtime Behavior

After boot, the Hi3861 serial output should include:

```text
10.0SUM project start.
Task 1 running: car safety: IR edge protection + ultrasonic obstacle avoidance
Task 2 running: OLED display: SHT20 temperature/humidity + AP3216C IR/ALS/PS
Task 3 running: Bluetooth UART1 communication
```

If the serial port still prints:

```text
AP3216C ir=..., als=..., ps=..., led=...
```

then the board is still running the old `9.0AP3216` firmware instead of `10.0SUM`.

## Hi3861 Build Target

Use the final project target:

```text
10.0SUM:SumCar
```

Do not build or burn:

```text
9.0AP3216:Ap3216c
```

for the final smart car project.

## STM32 Program

The STM32 side receives commands from Hi3861 through UART at `115200` baud.

Motor frame format:

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

Direction values:

```text
0 = forward
1 = reverse
```

Speed range:

```text
0 to 150
```

The STM32 side also supports LED commands:

```text
L1 = LED on
L0 = LED off
```

## Wiring

Hi3861:

- GPIO14: left infrared table-edge sensor
- GPIO13: right infrared table-edge sensor
- GPIO7: HC-SR04 Trig
- GPIO8: HC-SR04 Echo
- GPIO2: SG90 servo signal
- GPIO11: UART2 TX to STM32 RX
- GPIO12: UART2 RX from STM32 TX
- GPIO0: Bluetooth UART1 TX
- GPIO1: Bluetooth UART1 RX
- GPIO9: I2C0 SCL
- GPIO10: I2C0 SDA
- GPIO6: local LED output

STM32:

- USART1 RX/TX: PA10 / PA9
- Left motor PWM: PB7 / TIM4_CH2
- Left motor direction: PB14
- Right motor PWM: PB6 / TIM4_CH1
- Right motor direction: PB13
- WS2812 colorful LED control uses the existing `colorful_led` driver.

## Important Parameters

Infrared edge detection:

```c
#define IR_EDGE_VALUE HI_GPIO_VALUE1
```

If desktop and edge detection are reversed, change it to:

```c
#define IR_EDGE_VALUE HI_GPIO_VALUE0
```

Servo direction:

```c
#define SG90_LEFT_DUTY_US 2400
#define SG90_CENTER_DUTY_US 1650
#define SG90_RIGHT_DUTY_US 900
```

Obstacle threshold:

```c
#define OBSTACLE_DISTANCE_CM 20.0f
```

Edge protection timing:

```c
#define SAMPLE_PERIOD_MS 30
#define BRAKE_TIME_MS 300
#define BACKWARD_TIME_MS 50
#define EDGE_TURN_TIME_MS 180
```

Obstacle avoidance timing:

```c
#define OBSTACLE_BACKWARD_TIME_MS 50
#define OBSTACLE_TURN_TIME_MS 300
#define MAX_OBSTACLE_TURN_TIME_MS 900
```

## Notes

Both Hi3861 and STM32 firmware must be rebuilt and burned.

The current local machine did not have the required Hi3861 or STM32 compiler tools available, so the code was updated and statically checked, but firmware binaries were not rebuilt here.



# 9.2

## Purpose

The Hi3861 acts as the bridge between the mobile phone and the STM32 motor controller.

It receives Bluetooth commands from the phone and forwards motor control data to the STM32.

## Control Flow

```text
Mobile phone
  -> Bluetooth module
  -> Hi3861 UART1
  -> Hi3861 GPIO11 software UART
  -> STM32 USART1
  -> Motors
```

## Bluetooth UART

The Bluetooth module is connected to Hi3861 UART1.

```text
GPIO0: UART1_TX
GPIO1: UART1_RX
Baud rate: 9600
```

UART1 is initialized with:

```c
UartInit(WIFI_IOT_UART_IDX_1, &uartAttr, NULL);
```

The Hi3861 reads Bluetooth data using:

```c
UartRead(WIFI_IOT_UART_IDX_1, uartBuff, UART_BUFF_SIZE - 1);
```

## Mobile Command Protocol

The phone sends single ASCII characters:

```text
O: Stop
W: Move forward
A: Turn left
D: Turn right
S: Move backward
I: Low-speed forward
K: High-speed forward
```

## Command Processing

After the Hi3861 receives a command, it calls the matching control function:

```c
case 'O':
    CarStop();
    break;

case 'W':
    CarForward();
    break;

case 'A':
    CarLeft();
    break;

case 'D':
    CarRight();
    break;

case 'S':
    CarBackward();
    break;

case 'I':
    Stm32MotorControl(100, 100);
    break;

case 'K':
    Stm32MotorControl(150, 150);
    break;
```

Newline characters are ignored:

```c
case '\r':
case '\n':
    return;
```

## Forwarding to STM32

The Hi3861 does not directly drive the motors. Instead, it sends a motor control frame to the STM32.

Because UART1 is used by the Bluetooth module and UART2 cannot be initialized reliably at the same time in this SDK environment, GPIO11 is used as a software UART TX pin.

```text
GPIO11: software UART TX to STM32 RX
Baud rate: 9600
```

## Software UART

GPIO11 is configured as a normal output pin:

```c
IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_GPIO);
GpioSetDir(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_GPIO_DIR_OUT);
GpioSetOutputVal(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_GPIO_VALUE1);
```

One UART bit lasts about 104 microseconds:

```c
#define SOFT_UART_BIT_US 104
```

This matches a 9600 baud UART signal.

## Motor Frame Format

The Hi3861 sends a 6-byte frame to the STM32:

```text
0xFC  left_dir  left_speed  right_dir  right_speed  0xFD
```

Direction values:

```text
0: Forward
1: Reverse
```

Example: stop

```text
FC 00 00 00 00 FD
```

Example: move forward at speed 150

```text
FC 00 96 00 96 FD
```

## Command Mapping

```text
O -> 0, 0
W -> 150, 150
A -> -50, 150
D -> 150, -50
S -> -150, -150
I -> 100, 100
K -> 150, 150
```

## Reliability Improvements

To improve reliability, each command frame is sent multiple times.

```text
Normal commands: 3 times
Stop command: 8 times
```

The stop command is repeated more times to make stopping more responsive.

The Bluetooth read interval is reduced to:

```c
#define UART_READ_IDLE_US (10 * 1000)
```

This allows the Hi3861 to check for new Bluetooth commands every 10 ms.

## Startup Log

After successful initialization, the serial monitor should show:

```text
Bluetooth car control start.
```

When a command is received, it should show:

```text
UART recv: W
Bluetooth command: W
```

## Notes

- The mobile app should send plain ASCII characters.
- `\r` and `\n` are ignored.
- Hi3861 and STM32 must share GND.
- The STM32 UART baud rate must also be 9600.
- GPIO11 from Hi3861 should be connected to the STM32 UART RX pin.
