# STM32 motor controller

This STM32F103 Keil project receives motor commands from the Hi3861 board and drives the L9110S motor driver.

## Entry points

- `USER/main.c`: initializes clock, USART1, USART2, delay, and TIM4 PWM, then waits for UART commands.
- `SYSTEM/usart/usart.c`: parses the 6-byte motor frame from Hi3861.
- `QST_HARDWARE/motor/motor.c`: maps signed left/right motor speed to TIM4 PWM and direction GPIO.

## UART protocol

USART1 and USART2 are configured at 115200 baud, 8 data bits, no parity, 1 stop bit. Both ports accept the same motor frame, so the car still works if the Hi3861-to-STM32 link is wired to PA10 or PA3.

Frame format:

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

- `left_dir` / `right_dir`: `0` forward, `1` reverse.
- `left_speed` / `right_speed`: 0 to 150.

The receiver multiplies each speed byte by 20 before writing PWM, so speed 150 becomes PWM 3000.

## Motor pins

- Left PWM: PB7 / TIM4_CH2
- Left direction: PB14
- Right PWM: PB6 / TIM4_CH1
- Right direction: PB13

Connect Hi3861 UART2 TX (GPIO11) to STM32 USART1 RX (PA10) or STM32 USART2 RX (PA3), and share GND.
