# car-driving
summer school from 8.24 to 9.6

8.25
modifying the LED effect after receiving the UART command：
1. Added a dual-side running light effect————LR_runingled(); This function updates both the left and right WS2812 LED data buffers and refreshes both LED strips in each frame.
2. Added a blade-style LED effect————Instead of simply lighting one LED at a time, this effect uses a bright main LED and dimmer LEDs as a tail, making the LEDs look like a rotating blade.

8.26
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

