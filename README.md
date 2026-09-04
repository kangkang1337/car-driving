# 8.26

## Hi3861 Hello World and Basic Project Setup

This stage focused on basic Hi3861/OpenHarmony LiteOS-M verification, including serial logs, dual-thread tasks, build configuration, and firmware flashing.

Key points:

- The active demo is selected in `app/BUILD.gn` through the `features` list of `lite_component("app")`.
- The usual format is `"folder_name:target_name"`, for example `"1.0_Hello_World:hello_world"`.
- If the board still runs old firmware after flashing, first check whether `BUILD.gn` was saved, whether the selected target is correct, and whether the generated bin file is the latest one.
- Startup logs such as `ready to OS start`, `sdk ver`, `FileSystem mount ok`, and `wifi init success` are normal Hi3861 system startup messages.

## Dual-Thread Demo

Two tasks were created to verify CMSIS-RTOS2 thread creation and scheduling:

- Task 1 prints one set of logs periodically.
- Task 2 prints another set of logs periodically.
- Threads are created with `osThreadNew()`, and periodic timing is controlled with `osDelay()`.

Notes:

- In this Hi3861 project, `osDelay()` is not necessarily measured in 1 ms units. Later testing showed that one OS tick is about 10 ms.
- Alternating serial output from both tasks confirms that task scheduling is working.

## SG90 Servo Demo

The SG90 servo was tested with PWM output.

Key points:

- SG90 uses 50 Hz PWM, with a period of about 20 ms.
- The angle is controlled by changing the high-level pulse width. In general, about 0.5 ms to 2.5 ms maps to 0 to 180 degrees.
- In the later ultrasonic obstacle avoidance project, the SG90 drives the HC-SR04 module to scan left, front, and right.

## Flashing and Debugging

Common operations:

- Use HiBurn to flash Hi3861 firmware.
- BOOT, RESET, and switch states affect whether the board can enter flashing mode.
- If the serial output still shows the old program after flashing, the common causes are: the project was not rebuilt, `BUILD.gn` was not saved, or the flashed bin file was not the latest output.

# 8.27

## STM32 Motor Control Basics

This stage organized the STM32 chassis code, including motors, encoders, PID, timers, and the serial protocol.

Core STM32 motor structure:

- TIM4 outputs PWM to control the left and right motor speeds.
- Common left motor pins: PB7 and PB14.
- Common right motor pins: PB6 and PB13.
- `Set_Pwm()` writes the target PWM values into the timer compare registers.
- `stm32motor_control(left, right)` converts left and right speed values into direction and PWM output.

Notes:

- In the later Bluetooth project, the speed scale was found to be too small. The conversion inside `stm32motor_control()` was eventually adjusted to the level of `Set_Pwm(speed * 20)`, otherwise forward movement was too slow.
- If the motors do not move, check PWM output, direction pins, motor power, common ground, and whether serial data really reaches the STM32.

## Encoders and PID

The STM32 side kept the design for encoder feedback and PID control:

- Encoders measure the actual speed of the left and right wheels.
- Encoder increments are read periodically by a timer.
- PID calculates PWM corrections from the target speed and measured speed.

Debugging focus:

- Verify open-loop motor rotation first, then add encoder closed-loop control.
- If PID output is abnormal, check encoder direction, counting period, target speed units, and PWM limits.
- If left and right motor directions are inconsistent, fix the direction pin logic or the sign of the speed value first.

## HC-SR04 Tick Timing

Basic ultrasonic distance measurement was completed.

Key points:

- TRIG outputs the trigger pulse.
- ECHO input pulse width is used to calculate distance.
- System tick or timer counting is used to measure the ECHO high-level duration.
- A timeout is required so the program does not block forever when no echo is received.

Common problems:

- If distance is always `-1` or abnormal, ECHO usually did not receive a valid pulse. Possible causes include wiring, unstable power, or unreasonable timeout settings.
- HC-SR04 usually needs 5 V power. When ECHO returns to a 3.3 V chip, voltage compatibility must be considered.

# 8.28

## IR Edge Protection

Main project path:

- `C:\Users\18500\Desktop\summer\SSH-192.168.13.128\2.0Timer\test`

Goal:

- Use two IR sensors to detect the edge of a table.
- When an edge is detected, stop immediately, move backward, and turn to avoid falling.

Sensor definition:

- Left IR sensor: GPIO14.
- Right IR sensor: GPIO13.
- Safe state: left `0`, right `0`.
- Edge detected: the corresponding sensor input becomes `1`.

Control logic:

- The car moves forward in the normal state.
- If either side detects an edge, the car stops first.
- After a short brake delay, the car moves backward.
- The car then turns according to the triggered side.
- After the turn, it returns to the detection loop.

Implementation notes:

- Use `hi_gpio_get_input_val()` to read GPIO input values.
- Hi3861 does not drive the motors directly. It sends control frames to the STM32 through UART.
- The motor control frame is 6 bytes:

```text
FC dirL speedL dirR speedR FD
```

Field description:

- `FC`: frame header.
- `dirL`: left motor direction.
- `speedL`: left motor speed.
- `dirR`: right motor direction.
- `speedR`: right motor speed.
- `FD`: frame tail.

# 8.30

## Ultrasonic Obstacle Avoidance Car

Main project path:

- `C:\Users\18500\Desktop\summer\test\test2-sg90`

This stage completed the ultrasonic obstacle avoidance car using Hi3861 and STM32 together.

## Hi3861 Side

Hi3861 handles sensor detection and decision-making:

- SG90 servo: GPIO2.
- HC-SR04 TRIG: GPIO7.
- HC-SR04 ECHO: GPIO8.
- UART2 TX/RX: GPIO11 and GPIO12.
- UART baud rate: 115200.

Control flow:

- Send a stop command first after startup to prevent accidental movement.
- The servo rotates the ultrasonic module to scan front, left, and right distances.
- If the front distance is below the threshold, stop and compare the left and right distances.
- Turn in place toward the side with more free space.
- Continue forward movement and detection after turning.

Final obstacle threshold:

- Trigger obstacle avoidance when the obstacle is closer than about 20 cm.

Important debugging changes:

- Removed forced forward movement at startup to prevent the car from rushing forward after power-on.
- Removed the unstable backward fallback logic and changed to a more direct stop-and-turn behavior.
- Fixed the turning action so the car turns in place instead of turning while moving forward.

## STM32 Side

STM32 receives serial commands from Hi3861 and drives the chassis motors.

Core points:

- USART receives the 6-byte motor control frame.
- TIM4 outputs PWM.
- PB7 and PB14 control the left motor.
- PB6 and PB13 control the right motor.
- The sign of the left and right speed values determines forward, backward, and turning motion.

Debugging method:

- First test both motors with fixed PWM.
- Then use a serial tool or Hi3861 to send fixed frames and verify parsing.
- Finally connect the ultrasonic obstacle avoidance logic.

# 8.31

## AP3216C Light Detection and STM32 LED Control

Main project path:

- `C:\Users\18500\Desktop\summer\SSH-192.168.13.128\9.0AP3216`

This stage completed AP3216C ambient light detection and used Hi3861 serial output to control WS2812 LEDs on STM32.

## Hi3861 Side

Hardware connections:

- I2C0 SDA: GPIO10.
- I2C0 SCL: GPIO9.
- AP3216C I2C address: `0x3C`.
- OLED uses SSD1306.
- UART2 sends LED commands to STM32.
- UART2 baud rate: 9600.

Functions:

- Initialize AP3216C.
- Read IR, ALS, and PS data.
- Display sensor data on OLED.
- Control LED state according to ALS ambient light value.

Control rule:

- `ALS <= 50`: treat the environment as dark, send `L1\n`, and STM32 turns on the WS2812 LEDs.
- `ALS > 50`: treat the environment as bright, send `L0\n`, and STM32 turns off the WS2812 LEDs.

Important fix:

- The original logic only checked `als == 0`, so dark conditions with non-zero ALS values could not turn on the LEDs.
- It was later changed to `als <= 50`, which better matches real ambient light behavior.

## STM32 Side

STM32 works as the LED command receiver:

- USART1 baud rate: 9600.
- Receives `L1` or `L0` text commands.
- `L1`: turn all WS2812 LEDs white.
- `L0`: turn all WS2812 LEDs off.

Cleanup:

- Unrelated motor, encoder, and PWM code was removed to keep the LED control project clean.
- Only serial receiving and WS2812 control code was kept.

## AP3216C Debugging

Observed issue:

```text
AP3216C reset failed, err=0x80001182
```

Meaning:

- The I2C access did not receive an ACK, which usually means the device did not respond correctly.

Debugging direction:

- Check whether SDA and SCL are reversed.
- Check whether VCC and GND are stable.
- Check whether the AP3216C address is correct.
- Check whether the cable is loose.

At that time, reconnecting the cable fixed the issue.

# 9.1

## 10.0SUM Integrated Project

This stage integrated multiple previous modules into one smart car project.

Main functions:

- IR edge protection.
- HC-SR04 ultrasonic obstacle avoidance.
- OLED sensor display.
- SHT20 temperature and humidity reading.
- AP3216C IR, ALS, and PS reading.
- WiFi connection.
- Huawei Cloud MQTT real-time data upload.
- Java Web dashboard receiving cloud data through AMQP.

Uploaded data fields:

- `temperature`: temperature.
- `humidity`: humidity.
- `ap_ir`: AP3216C IR value.
- `ap_als`: AP3216C ambient light value.
- `ap_ps`: AP3216C proximity value.
- `edge_left`: left edge detection state.
- `edge_right`: right edge detection state.
- `distance_cm`: ultrasonic distance.
- `led_on`: LED state.

## Hi3861 Integrated Logic

Task division:

- Task 1: car safety control, including IR edge protection and ultrasonic obstacle avoidance.
- Task 2: OLED display for temperature, humidity, AP3216C data, and LED state.
- Task 3: WiFi scanning, connection, and DHCP.
- Task 4: Huawei Cloud MQTT connection and data upload.

The following startup logs indicate that the integrated project is running correctly:

```text
10.0SUM project start.
Task 1 running: car safety
Task 2 running: OLED display
Task 3 running: WiFi connect
Task 4 running: Huawei Cloud MQTT realtime upload
```

## Huawei Cloud Connection

MQTT connection return code issue:

- `CONNACK code=4` means the username or password is incorrect, or the clientId/authentication parameters do not match the platform requirements.
- The issue was later fixed by adjusting the clientId timestamp format.

WiFi connection flow:

- Scan for the target hotspot.
- Connect after the target SSID is found.
- Allow MQTT connection only after DHCP succeeds.
- Upload JSON data periodically after MQTT is connected.

## Web Dashboard

Main project path:

- `C:\Users\18500\Desktop\summer\test\test3-html`

Main files:

- `car.html`: frontend page.
- `CarCloudServer.java`: Java backend, receives AMQP data and forwards it to the page.
- `pom.xml`: Maven configuration.
- `README_WEB.md`: Web-side documentation.

Access URL:

```text
http://localhost:8080/car.html
```

AMQP information:

- Queue: `qst_queue`.
- Huawei Cloud application-side host contains `iotda-app.cn-north-4`.
- Port: 5671.

## Delay Issue

An important timing issue was found during debugging:

- In the current Hi3861 project, one `osDelay()` tick is about 10 ms.
- Therefore, `osDelay(5000)` is about 50 seconds, not 5 seconds.

Fix:

- Added a `DelayMs()` wrapper function.
- Defined `OS_TICK_MS 10`.
- All delays that should be treated as milliseconds were changed to use `DelayMs()`.
- Data upload period was adjusted to about 2 seconds.

## Edge Protection Jitter Fix

Problem:

- The car behaved unstably near the table edge. It could move backward slightly and then trigger edge detection again.

Adjusted parameters:

- `BACKWARD_TIME_MS 350`.
- `EDGE_TURN_TIME_MS 350`.
- `EDGE_RELEASE_TIME_MS 250`.

Result:

- The car moves backward farther after detecting an edge.
- Turning and release timing are more stable.
- The car is less likely to get stuck repeatedly near the edge.

# 9.2

## Bluetooth Phone Control Car

Main project path:

- `C:\Users\18500\Desktop\summer\test\test4-bluetooth`

Goal:

- The phone connects to the Bluetooth module through a Bluetooth debugging app.
- The Bluetooth module converts the character sent by the phone into serial data for Hi3861.
- Hi3861 parses the character command.
- Hi3861 sends a motor control frame to STM32.
- STM32 finally drives the car motors.

Overall data path:

```text
iPhone LightBlue
    -> BLE Bluetooth module
    -> Hi3861 UART1 RX
    -> Hi3861 software UART TX
    -> STM32 USART1 RX
    -> STM32 motor PWM
```

## Phone-Side Protocol

The phone sends one ASCII character each time:

```text
O: stop
W: forward
A: turn left
D: turn right
S: backward
I: left/right speed 100,100
K: left/right speed 150,150
```

Corresponding control logic:

```c
case 'O':
    car_stop();
    break;
case 'W':
    car_forward();
    break;
case 'A':
    car_left();
    break;
case 'D':
    car_right();
    break;
case 'S':
    car_backward();
    break;
case 'I':
    stm32motor_control(100, 100);
    break;
case 'K':
    stm32motor_control(150, 150);
    break;
```

## iOS LightBlue Usage

LightBlue can be used for iOS debugging, but two conditions must be met:

- The Bluetooth module must be a BLE module. iPhone does not support classic Bluetooth SPP serial communication.
- After connecting to the device in LightBlue, find a writable characteristic and write `W`, `A`, `D`, `O`, and other commands as ASCII or UTF-8 text.

If LightBlue cannot find the car:

- Confirm that the Bluetooth module is BLE.
- Confirm that the module is powered and advertising.
- Confirm that the Hi3861 program did not exit early because UART initialization failed.
- If the module is already connected to another phone, disconnect it first and then scan again.

## Hi3861 Side

The first implementation used hardware UART:

- UART1 connected to the Bluetooth module and received phone commands.
- UART2 connected to STM32 and sent motor control frames.

Later, these errors appeared:

```text
Failed to init UART1, err code: 4294967295
Failed to init motor UART2, err code: 4294967295
```

`4294967295` is actually `-1` printed as an unsigned value, meaning UART initialization failed.

Diagnosis:

- In the current Hi3861 project or board-level pin configuration, initializing UART1 and UART2 together was unstable.
- Once UART initialization failed, the program could not reliably receive Bluetooth data or send motor control frames.

Final solution:

- UART1 is used only to receive data from the Bluetooth module.
- The second hardware UART was abandoned.
- GPIO11 is used as a software UART TX pin to send motor control frames to STM32.

Software UART parameters:

- Baud rate: 9600.
- One bit time: about 104 us.
- TX pin: GPIO11.
- Idle level: high.
- Start bit: low.
- Data bits: 8 bits, LSB first.
- Stop bit: high.

Reliability improvements:

- Normal motion commands send the motor frame 3 times.
- The stop command `O` sends the motor frame 8 times.
- `osKernelLock()` is used while sending software UART bytes to reduce timing interference from task switching.
- The Bluetooth receive loop was shortened to about 10 ms to improve responsiveness.

## Hi3861-to-STM32 Frame Format

The 6-byte protocol is still used:

```text
FC dirL speedL dirR speedR FD
```

Speed and direction convention:

- Positive value: forward.
- Negative value: backward.
- `0`: stop.

Final common actions on 9.2:

```text
O -> 0, 0
W -> 150, 150
A -> -50, 150
D -> 150, -50
S -> -150, -150
I -> 100, 100
K -> 150, 150
```

Notes:

- `A` and `D` use opposite wheel directions to turn in place.
- The forward speed for `W` was increased to 150 to fix slow forward movement.
- The stop command `O` repeats the frame more times to fix delayed stopping.

## STM32 Side

STM32 receives the software UART data from Hi3861.

Key configuration:

- USART1.
- Baud rate: 9600.
- Receives data from Hi3861 GPIO11.
- GND must be shared between Hi3861 and STM32.

STM32 receive logic:

- Parses the 6-byte motor control frame.
- Can also support single-character commands for direct serial-tool testing.
- Calls `stm32motor_control(left, right)` after receiving a valid frame.

Speed fix:

- At first, the STM32 speed scale was too small, so forward movement was very slow.
- The PWM conversion ratio inside `stm32motor_control()` was increased.
- After using `Set_Pwm(speed * 20)`, the car moved forward normally.

## Why Only A, D, and O Worked at First

Debugging conclusion:

- Bluetooth receiving on Hi3861 was basically working because the serial logs showed `UART recv: W` and `UART recv: O`.
- The main problem was not the phone protocol. It was the sending link from Hi3861 to STM32 and the STM32 PWM speed scale.
- `A`, `D`, and `O` were easier to observe because turning and stopping produced more obvious motor-state changes.
- `W`, `S`, `I`, and `K` were more affected by speed scaling, frame loss, and receive timing.

The final improvements were:

- Use Hi3861 software UART TX to avoid UART1/UART2 initialization conflicts.
- Reduce baud rate to 9600 to improve software UART reliability.
- Send control frames repeatedly.
- Repeat the stop command more times.
- Increase the STM32 PWM output scale.

## Alternatives to Software UART

If software UART is not used, possible alternatives include:

- Reorganize hardware UART resources so Bluetooth and STM32 use separate available UARTs. This requires confirming that the current Hi3861 SDK and board pin-mux configuration support it.
- Connect the phone Bluetooth module directly to STM32, letting STM32 parse Bluetooth commands and drive the motors. This bypasses Hi3861, so it does not match the project structure of "phone connects to Hi3861, then Hi3861 controls STM32".
- Use I2C, with Hi3861 as master and STM32 as slave. Hi3861 sends speed commands to STM32 through I2C.
- Use SPI, with Hi3861 as master and STM32 as slave. SPI is faster but needs more wiring and a more complex protocol.
- Use single-wire GPIO pulse encoding. This is only suitable for very few commands and is not good for scalable speed data.

## I2C Option

If the link is changed to I2C:

- Hi3861 acts as the I2C master.
- STM32 acts as the I2C slave.
- After Hi3861 receives a Bluetooth character from the phone, it writes one command frame through I2C instead of using software UART.
- STM32 receives the command through I2C interrupt or polling, then calls `stm32motor_control()`.

The same frame format can still be used:

```text
FC dirL speedL dirR speedR FD
```

Advantages:

- Only SDA, SCL, and GND are required.
- The master-slave structure is clear, and Hi3861 is suitable as the central controller.
- It does not depend on precise bit-bang timing, so it is more stable than software UART.

Disadvantages:

- STM32 I2C slave configuration is more complex than normal UART receiving.
- Address, ACK, timing, and interrupt handling must be configured correctly.
- The I2C bus needs pull-up resistors, and long wires can make it unstable.
- If OLED, AP3216C, or other devices are also on the bus, address conflicts and bus blocking must be avoided.

Suitable when:

- More Hi3861-to-STM32 control data needs to be added later.
- UART resource conflicts need to be reduced.
- Adding STM32 I2C slave receive code is acceptable.

## Final 9.2 Conclusion

The current most usable solution:

- The phone uses LightBlue to send single-character commands.
- The Bluetooth module sends the character to Hi3861 through UART1.
- Hi3861 parses `O/W/A/D/S/I/K`.
- Hi3861 sends motor frames to STM32 through GPIO11 software UART.
- STM32 USART1 receives the frames and drives the motors.

This solution keeps the project structure of "phone connects to Hi3861, then Hi3861 controls STM32" while avoiding the Hi3861 dual-hardware-UART initialization problem.


# 2026-09-03 README

## Project: test6-way Line Following Experiment

Today’s work focused on the `test6-way` line-following experiment for the QST smart car.

The experiment path is:

```text
C:\Users\18500\Desktop\summer\test\test6-way
```

## Goal

The car follows a black line using two infrared sensors.

The black line should stay between the two IR sensors. The sensors should not ride directly on the black line during normal forward movement.

Sensor values:

```text
Floor: 0
Black line: 1
```

There are branch paths on the track. A wrong branch ends with one horizontal black line. The correct finish area has two horizontal black lines, with a white gap between them.

Finish mark size:

```text
First black line width: about 1.9 cm
White gap: about 1.5 cm
Second black line width: about 1.9 cm
```

When the car reaches the correct finish mark, it stops and flashes all car lights.

## Reference Project

The main reference project was:

```text
C:\Users\18500\Desktop\summer\test\test5-blacktape
```

The `test5-blacktape` project was copied into the `test6-way` working directory before modification.

Unrelated modules were removed to reduce interference:

```text
WiFi
Bluetooth
Huawei Cloud
OLED
SHT20
AP3216C
Ultrasonic sensor
SG90 servo
PID
Encoder
```

## Hi3861 Side

The Hi3861 program is:

```text
way.c
```

The build target is:

```gn
static_library("way")
```

The parent OpenHarmony application `BUILD.gn` should enable:

```gn
"13.0_way:way",
```

or the matching folder name used in the Linux OpenHarmony workspace.

Main hardware usage:

```text
Left IR sensor: GPIO13
Right IR sensor: GPIO14
Motor UART to STM32: UART2
UART2 TX: GPIO11
UART2 RX: GPIO12
Baud rate: 115200
```

The Hi3861 reads the two IR sensors and sends motor control frames to STM32.

Motor frame format:

```text
0xFC left_dir left_speed right_dir right_speed 0xFD
```

## Line Following Logic

The main loop polls the IR sensors every `10 ms`.

Normal states:

```text
0,0: line is between the two sensors, move forward
1,0: left sensor sees black, bias left
0,1: right sensor sees black, bias right
1,1: both sensors see black, handle as a marker/junction event
```

Speed settings:

```c
#define CRUISE_SPEED 125
#define CORRECT_FAST_SPEED 145
#define CORRECT_SLOW_SPEED 55
#define SAMPLE_PERIOD_MS 10
```

The correction is not a long fixed turn. The car sends one short correction command, polls the sensors again, and immediately adjusts based on the latest sensor state.

## Finish Detection

The finish mark is detected as a black-white-black pattern.

Logic:

```text
1. Detect first horizontal black line: both sensors read 1,1
2. Move forward slowly
3. Once the sensors leave the first black line, start checking for the second black line
4. If another 1,1 is detected within the short finish window, treat it as the finish
5. Stop the car and flash all lights
```

Important parameters:

```c
#define FINISH_DETECT_SPEED 80
#define FINISH_FIRST_RELEASE_TIMEOUT_MS 350
#define FINISH_WHITE_GAP_MAX_MS 350
#define FINISH_SECOND_CONFIRM_MS 10
```

The previous `FINISH_WHITE_GAP_MIN_MS` check was removed because setting it to `0` caused a compiler warning:

```text
comparison of unsigned expression >= 0 is always true
```

Since warnings are treated as errors in the Hi3861 build, this stopped compilation.

## Junction Handling

During testing, normal branch intersections were often read as `1,1`, the same as a horizontal marker.

The strategy was changed:

```text
First, check whether 1,1 is the finish black-white-black pattern.
If not finish, treat it as a junction.
```

Junction decision rule:

```text
1st junction marker: bias left
2nd junction marker: bias right
Further junction markers: alternate by odd/even count
```

Junction parameters:

```c
#define JUNCTION_LEFT_FAST_SPEED 145
#define JUNCTION_LEFT_SLOW_SPEED 55
#define JUNCTION_RIGHT_FAST_SPEED 145
#define JUNCTION_RIGHT_SLOW_SPEED 55
#define JUNCTION_BIAS_MS 260
#define JUNCTION_RELEASE_TIMEOUT_MS 700
```

The previous dead-end backtracking logic was removed from the active version because the real track behavior showed that branch intersections were being detected as double-black junction events.

## STM32 Side

The STM32 project is:

```text
C:\Users\18500\Desktop\summer\test\test6-way\TIMER\USER\TIMER.uvprojx
```

STM32 keeps only the required modules:

```text
USART1 receiver
TIM4 PWM motor control
WS2812 car light control
```

STM32 receives motor frames from Hi3861 and drives the motors.

STM32 also supports the finish command:

```text
E\n
```

When STM32 receives `E\n`, it stops the motors and flashes all WS2812 lights.

## Current Status

Completed:

```text
Created test6-way from test5-blacktape
Removed unrelated modules
Implemented high-frequency IR polling
Implemented slow line following
Implemented stronger short correction
Implemented black-white-black finish detection
Implemented junction count logic
Implemented finish light flashing command
Fixed Hi3861 build warning caused by unsigned comparison
Updated STM32 UART command handling
```

Current behavior:

```text
The car follows the line slowly.
When both sensors read black, it first checks for the finish pattern.
If not finish, it treats the event as a junction.
The first junction biases left.
The second junction biases right.
At the finish black-white-black marker, the car stops and flashes all lights.
```

## Tuning Notes

If line following is unstable:

```c
CRUISE_SPEED
CORRECT_FAST_SPEED
CORRECT_SLOW_SPEED
SAMPLE_PERIOD_MS
```

If the car misses the finish mark:

```c
FINISH_DETECT_SPEED
FINISH_WHITE_GAP_MAX_MS
FINISH_SECOND_CONFIRM_MS
```

If the car turns too much or too little at junctions:

```c
JUNCTION_BIAS_MS
JUNCTION_LEFT_FAST_SPEED
JUNCTION_LEFT_SLOW_SPEED
JUNCTION_RIGHT_FAST_SPEED
JUNCTION_RIGHT_SLOW_SPEED
```

If the correction direction is reversed, swap the left and right motor speed values in the correction functions.


# 9.4

## Project: test7-newway Line Following Experiment

Today’s work continued on the `test7-newway` line-following experiment for the QST smart car.

Project path:

```text
C:\Users\18500\Desktop\summer\test\test7-newway
```

## Goal

The car uses two infrared sensors to follow a black line. During normal driving, the black line should stay between the two sensors.

Sensor values:

```text
Floor: 0
Black line: 1
```

The track includes branch intersections and a finish marker.

## Hardware Mapping

Hi3861 side:

```text
Left IR sensor: GPIO13
Right IR sensor: GPIO14
UART2 TX to STM32: GPIO11
UART2 RX: GPIO12
Baud rate: 115200
```

STM32 side:

```text
USART1 receives motor and light commands from Hi3861
TIM4 controls motor PWM
WS2812 LEDs are used as car lights
```

## Line Following

The main loop polls the IR sensors every `10 ms`.

Basic sensor logic:

```text
0,0: line is between the two sensors, move forward
1,0: left sensor detects black, correct left
0,1: right sensor detects black, correct right
1,1: marker or branch intersection detected
```

Current speed settings:

```c
#define CRUISE_SPEED 125
#define CORRECT_FAST_SPEED 145
#define CORRECT_SLOW_SPEED 55
#define SAMPLE_PERIOD_MS 10
```

The correction logic sends short motor commands and immediately polls the sensors again. This keeps the car from making long blind turns and improves line-following stability at low speed.

## Marker and Junction Handling

During testing, branch intersections were often detected as `1,1`, the same as a horizontal marker.

The strategy was adjusted so that `1,1` is handled as a marker event:

```text
First, check whether the marker is the finish pattern.
If not finish, treat it as a branch intersection.
```

Branch behavior:

```text
1st non-finish marker: bias left
2nd non-finish marker: bias right
Later non-finish markers: alternate by odd/even count
```

The branch turn time was increased because the car was almost turning correctly but still slightly short at each branch.

Current branch parameters:

```c
#define JUNCTION_LEFT_FAST_SPEED 145
#define JUNCTION_LEFT_SLOW_SPEED 55
#define JUNCTION_RIGHT_FAST_SPEED 145
#define JUNCTION_RIGHT_SLOW_SPEED 55
#define JUNCTION_BIAS_MS 380
#define JUNCTION_RELEASE_TIMEOUT_MS 700
```

`JUNCTION_BIAS_MS` was increased from `260 ms` to `380 ms`.

## Finish Detection

The finish marker was restored to a black-white-black pattern.

Physical finish marker:

```text
First black line width: about 1.9 cm
White gap: about 1.5 cm
Second black line width: about 1.9 cm
```

Finish detection flow:

```text
1. Detect first 1,1 marker
2. Move forward slowly
3. Wait until the sensors leave the first black line
4. Look for a second 1,1 marker within a short time window
5. If the second marker is found, stop the car and flash all lights
6. If the second marker is not found, treat the event as a branch intersection
```

Current finish parameters:

```c
#define FINISH_DETECT_SPEED 80
#define FINISH_FIRST_RELEASE_TIMEOUT_MS 350
#define FINISH_WHITE_GAP_MAX_MS 350
#define FINISH_SECOND_CONFIRM_MS 10
```

The earlier “third detected black line means finish” logic was removed. The finish is now determined only by the black-white-black structure.

## Build Fix

A previous build error was caused by this kind of comparison:

```c
whiteElapsed >= FINISH_WHITE_GAP_MIN_MS
```

When `FINISH_WHITE_GAP_MIN_MS` was `0`, the compiler reported:

```text
comparison of unsigned expression >= 0 is always true
```

Because the Hi3861 build treats warnings as errors, this stopped compilation.

The unused minimum-gap macro and the always-true comparison were removed. The current code does not use:

```c
FINISH_WHITE_GAP_MIN_MS
```

## STM32 Behavior

STM32 receives control commands from Hi3861.

Motor frame format:

```text
0xFC left_dir left_speed right_dir right_speed 0xFD
```

Finish command:

```text
E\n
```

When STM32 receives `E\n`, it stops the motors and flashes all WS2812 car lights.

## Current Status

Completed today:

```text
Restored black-white-black finish detection
Removed the third-marker finish rule
Increased branch turning time
Kept high-frequency 10 ms IR polling
Kept short correction commands for stable line following
Fixed the unsigned comparison build issue
Updated README behavior notes
```

Current behavior:

```text
The car follows the line slowly.
At a 1,1 marker, it first checks for the finish pattern.
If the finish pattern is detected, the car stops and flashes all lights.
If not finish, the car treats the marker as a branch intersection.
The first branch biases left.
The second branch biases right.
The branch bias time is longer than before to help the car complete the turn.
```

## Tuning Notes

If the car still does not turn enough at branches, increase:

```c
JUNCTION_BIAS_MS
```

Suggested next value:

```c
#define JUNCTION_BIAS_MS 450
```

If the car turns too much, reduce it back toward:

```c
#define JUNCTION_BIAS_MS 320
```

If the finish marker is missed, tune:

```c
FINISH_DETECT_SPEED
FINISH_WHITE_GAP_MAX_MS
FINISH_SECOND_CONFIRM_MS
```

If the car loses the main line, tune:

```c
CRUISE_SPEED
CORRECT_FAST_SPEED
CORRECT_SLOW_SPEED
SAMPLE_PERIOD_MS
```
