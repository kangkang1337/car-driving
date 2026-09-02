# test2-sg90 obstacle avoidance car

This project separates the Hi3861 decision/sensor code and the STM32 motor-driver code.

## Directory layout

- `Hi3861/`: HC-SR04 + SG90 obstacle avoidance logic. It sends motor commands to STM32 through UART2.
- `STM32/`: STM32F103 Keil project based on the existing QST car template. It receives UART frames and drives the L9110S motor PWM.

## Wiring used by the code

Hi3861:

- SG90 signal: GPIO2
- HC-SR04 Trig: GPIO7
- HC-SR04 Echo: GPIO8
- UART2 TX to STM32 RX: GPIO11
- UART2 RX from STM32 TX: GPIO12
- Baud rate: 115200, 8N1

STM32:

- USART1 RX/TX: PA10/PA9
- USART2 RX/TX: PA3/PA2
- Left motor PWM: PB7 / TIM4_CH2
- Left motor direction: PB14
- Right motor PWM: PB6 / TIM4_CH1
- Right motor direction: PB13

## Motor UART frame

Hi3861 sends a 6-byte frame:

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

- `dir = 0`: forward
- `dir = 1`: reverse
- `speed`: 0 to 150

STM32 multiplies `speed` by 20 and writes the result to the TIM4 PWM channels.

## Behavior

The car moves forward by default. When the front HC-SR04 distance is valid and less than or equal to 20 cm, the car stops, keeps the SG90 ultrasonic head centered, spins in place, and checks the front distance during the turn. It continues forward only after the front distance is greater than 20 cm or the maximum turn time is reached. If the obstacle is still within 20 cm after the turn, it backs up briefly before trying again.
