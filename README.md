# car-driving
summer school from 8.24 to 9.6

# 8.25
## modifying the LED effect after receiving the UART command：
1. Added a dual-side running light effect————LR_runingled(); This function updates both the left and right WS2812 LED data buffers and refreshes both LED strips in each frame.
2. Added a blade-style LED effect————Instead of simply lighting one LED at a time, this effect uses a bright main LED and dimmer LEDs as a tail, making the LEDs look like a rotating blade.

# 8.26
# STM32F103 PWM Motor Control

This Keil project controls the QST car motors with an STM32F103C8T6 and two L9110S motor driver chips. The project is based on the existing serial-print template and reuses its startup files, STM32 standard peripheral library, delay module, USART module, and colorful LED module.

## Hardware Mapping

Motor driver pins are taken from the schematic:

| Function | STM32 Pin | Timer Channel |
| --- | --- | --- |
| Left motor PWM, L-IA | PB7 | TIM4_CH2 |
| Left motor direction, L-IB | PB14 | GPIO output |
| Right motor PWM, R-IB | PB6 | TIM4_CH1 |
| Right motor direction, R-IA | PB13 | GPIO output |

The motor driver power rail is `M-5V`. The car will not move if the board power switch or motor power supply is off, even when the STM32 program is running.

## Project Files

- `USER/main.c`: main motion sequence and serial stop command check.
- `QST_HARDWARE/motor/motor.c`: PWM initialization and L9110S motor output control.
- `QST_HARDWARE/motor/motor.h`: motor API and PWM range definition.
- `SYSTEM/usart/usart.c`: USART1 initialization and `HELLO` command parsing.

## PWM Settings

The current PWM range is:

```c
#define PWM_MAX 3599
```

The program calls:

```c
PWM_Init(PWM_MAX, 0);
```

With a 72 MHz system clock, the PWM frequency is:

```text
72 MHz / ((0 + 1) * (3599 + 1)) = 20 kHz
```

This frequency was selected to reduce audible motor noise compared with the original 1 kHz setting.

## Motor API

Use `Set_Pwm(left_motor, right_motor)` to control both motors:

```c
Set_Pwm(3000, 3000);    // forward
Set_Pwm(0, 0);          // stop
Set_Pwm(-3000, -3000);  // backward
Set_Pwm(3000, -3000);   // rotate right in place
Set_Pwm(3000, 1800);    // curve right
```

The valid speed range is approximately `-3599` to `3599`. Positive and negative values select opposite directions.

## Current Motion Sequence

The current program runs a closed curve with a head-swing motion:

1. Start boost.
2. Right curve.
3. Head swing.
4. Straight transition.
5. Large left curve.
6. Head swing.
7. Straight transition.
8. Right curve to return near the start.

The start boost is used because the car needs a relatively high PWM value to overcome static friction:

```c
#define START_PWM 3300
```

## Serial Stop Command

USART1 is configured at `115200` baud. Sending uppercase `HELLO` from the serial assistant sets a stop flag. The main loop checks this flag every 20 ms during motion.

When `HELLO` is received, the car stops for 10 seconds and then continues the programmed motion sequence.

## Tuning Notes

- If the car does not move, check the motor power switch and `M-5V` supply first.
- If only one wheel moves, increase the slower side PWM.
- If the car turns too sharply, increase the slower wheel PWM.
- If the curve is too wide, decrease the slower wheel PWM.
- If the motor noise is high, keep PWM near or above 20 kHz.
- Avoid switching between forward and reverse too quickly; short intervals such as 100 ms can make the car jerk instead of moving smoothly.

# 8.27

## Overview

This STM32 workspace contains several small embedded projects for the QST smart car platform. The projects are based on STM32F10x standard peripheral libraries and are intended for Keil uVision development.

The main STM32 projects include:

- `PWM`: basic PWM motor output
- `TIMER`: encoder and timer-based motor speed output
- `PID`: motor speed closed-loop PID control
- `NFC`: NFC-related project structure
- `2_串口收发打印`: UART print and receive test project

## Hardware Platform

Target platform:

- MCU: STM32F103 series
- Motor driver: L9110S
- Left motor PWM: `TIM4_CH2 / PB7`
- Left motor direction: `PB14`
- Right motor PWM: `TIM4_CH1 / PB6`
- Right motor direction: `PB13`
- Left encoder: `TIM2`
- Right encoder: `TIM3`
- RGB LEDs: WS2812 colorful LED module
- UART: used for debug output

## Project Structure

Typical project layout:

```text
PROJECT_NAME/
├── CORE/
├── OBJ/
├── QST_HARDWARE/
│   ├── colorful_led/
│   ├── encoder/
│   ├── motor/
│   └── SYSTEM_CONTROL/
├── STM32F10x_FWLib/
├── SYSTEM/
│   ├── delay/
│   ├── sys/
│   └── usart/
└── USER/
    ├── main.c
    ├── stm32f10x_it.c
    └── *.uvprojx
```

## PID Project

The `PID` project implements motor speed closed-loop control.

Main files:

```text
PID/USER/main.c
PID/USER/stm32f10x_it.c
PID/QST_HARDWARE/SYSTEM_CONTROL/control_system.c
PID/QST_HARDWARE/SYSTEM_CONTROL/control_system.h
PID/QST_HARDWARE/motor/motor.c
PID/QST_HARDWARE/encoder/encoder.c
```

### Control Flow

1. `main.c` initializes the system clock, UART, delay, encoders, PWM, LEDs, and SysTick.
2. SysTick runs every 1 ms.
3. `stm32f10x_it.c` calls `System_Control()` every 100 ms.
4. `System_Control()` reads encoder values, calculates PID output, and updates motor PWM.

### Serial Output

The PID project prints debug information through UART:

```text
left  coder
right coder
TageA coder
TageB coder
Motor_A pwm
Motor_B pwm
State
Distance
```

Meaning:

- `left coder`: measured left encoder count during one control period
- `right coder`: measured right encoder count during one control period
- `TageA coder`: target encoder count for the left motor
- `TageB coder`: target encoder count for the right motor
- `Motor_A pwm`: PID PWM output for the left motor
- `Motor_B pwm`: PID PWM output for the right motor
- `State`: current motion state
- `Distance`: accumulated encoder count for the current motion stage

## Motion Logic

The current PID project supports a simple movement sequence:

```text
1. Move forward about 1 meter
2. Stop for about 0.5 seconds
3. Move backward about 1 meter
4. Stop
```

The distance is estimated from encoder counts:

```c
#define DRIVE_DISTANCE_COUNTS 14000
```

If the actual distance is too short, increase this value.  
If the actual distance is too long, decrease this value.

The pause time is controlled by:

```c
#define DRIVE_PAUSE_TICKS 5
```

The control period is 100 ms, so `5` ticks is about 0.5 seconds.

## Speed Settings

Forward speed:

```c
#define TARGET_LEFT_RPS       2.2f
#define TARGET_RIGHT_RPS      2.3f
```

Backward speed:

```c
#define BACKWARD_LEFT_RPS     2.7f
#define BACKWARD_RIGHT_RPS    2.8f
```

Backward startup compensation:

```c
#define BACKWARD_MIN_PWM      1600
```

Increase `BACKWARD_MIN_PWM` if the car shakes or cannot start backward.  
Decrease it if the backward motion starts too aggressively.

## PID Parameters

PID parameters are located in `control_system.c`.

Each motor has its own incremental PID function:

```c
int Incremental_PID_A(int encoder, int target)
int Incremental_PID_B(int encoder, int target)
```

Default parameters:

```c
const float kp = 7.0f;
const float ki = 0.016f;
const float kd = 0.003f;
```

Tuning suggestions:

- Increase `kp` if the response is too slow.
- Decrease `kp` if the motor oscillates.
- Increase `ki` if the motor cannot reach the target speed.
- Decrease `ki` if the speed drifts or overshoots.
- Increase `kd` slightly if the response is too aggressive.
- Set `kd` closer to zero if encoder noise causes jitter.

## RGB LED Effects

The colorful LED module uses WS2812 LEDs.

Main file:

```text
QST_HARDWARE/colorful_led/colorful_led.c
```

The `LR_rainbow()` function implements a rainbow running-light effect. The LEDs display a six-color rainbow sequence and rotate continuously.

Speed can be adjusted by changing:

```c
delay_ms(100);
```

Smaller delay means faster animation.

## Build and Flash

Open the corresponding `.uvprojx` file in Keil uVision.

For the PID project:

```text
PID/USER/PID.uvprojx
```

Build the project, then download it to the STM32 board using the configured debugger.

## Notes

- Do not change unrelated project files unless necessary.
- Keep motor direction signs consistent with the actual wiring.
- If the car moves in the wrong direction, reverse the target speed sign or adjust motor wiring.
- If PWM output reaches `PWM_MAX` but speed is still too low, the motor may be underpowered or overloaded.
- Encoder direction may differ between left and right wheels depending on wiring.
- For stable motor testing, ensure the battery voltage and motor driver power are sufficient.


# 8.28

Today I implemented and tested the NFC card control logic for the STM32 car project.

The NFC module can now read different card IDs correctly through the PN532 UART interface. The received card ID bytes are checked from `buf[19]` to `buf[22]`.

Current card behavior:

- `CB:98:A6:05`: moves the car forward
- `63:31:47:06`: stops the car and toggles the LED state
- `50:84:FC:23`: LED control card
- `40:74:80:23`: LED control card

The motor driver from the PWM project was added into the NFC project. The NFC project now initializes PWM before starting the NFC scan loop.

Main motor-related changes:

```c
PWM_Init(PWM_MAX, 9);
```

When the forward card is detected, the car first uses full PWM for a short startup boost, then keeps moving forward with a stable PWM value:

```c
Set_Pwm(FORWARD_START_PWM, FORWARD_START_PWM);
delay_ms(300);
Set_Pwm(FORWARD_PWM, FORWARD_PWM);
```

When the stop card is detected, the motor stops immediately:

```c
Motor_Stop();
```

The stop card also keeps the previous LED toggle behavior, so scanning it will stop the car and switch the LED state.

Debug messages were added to help verify the program flow through the serial monitor:

```text
PWM init ok
NFC wake up ok
NFC forward card
NFC stop card
NFC led card
```

After testing, the NFC card recognition worked correctly. The motor did not move at first because the car power switch was not turned on. After enabling the power, the NFC forward logic was confirmed to work.
