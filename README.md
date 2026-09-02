# car-driving
summer school from 8.24 to 9.6


# 2026.08.25 
学习笔记 今天主要学习了 STM32 串口通信、程序烧录、串口助手使用，以及 WS2812 彩灯控制代码的修改。

一、STM32 程序烧录

使用 STM32 ST-LINK Utility 连接开发板，通过 SWD 接口识别到 STM32F103C8T6 芯片。 烧录流程为：

打开生成的 Template.hex 文件。
通过 SWD 连接 STM32。
选择 Program & Verify 进行烧录和校验。
烧录完成后执行 System Reset，让程序重新运行。
二、串口助手使用

使用 UartAssist 进行串口通信测试。 串口参数设置为： 波特率：115200 数据位：8 停止位：1 校验位：None 流控：None

三、WS2812 彩灯控制原理 WS2812 是一种单总线控制的 RGB 彩灯。程序通过控制 GPIO 高低电平持续时间来发送 0 和 1。 原来的程序只调用 L_runingled()，所以只有一边灯会亮。后来添加了 LR_runingled()，在每一帧中同时设置左边和右边灯的数据，并同时刷新两边灯带。

四、今天完成的修改 今天主要完成了两个灯效修改：

添加左右两边同时跑马灯效果 新增 LR_runingled()，让左右两边 WS2812 灯同时刷新。
添加叶片灯效果 不再只是单颗灯依次点亮，而是使用一颗高亮灯加上较暗的拖尾灯，形成类似旋转叶片的视觉效果。
五、总结 今天学习了从代码编译、hex 文件烧录，到串口助手发送命令，再到 STM32 接收命令后控制 WS2812 灯效的完整流程。 通过调试过程，理解了：

ST-LINK 主要用于程序下载和调试。
CH340 负责 USB 转串口通信。
串口助手发送数据前必须先打开串口。
printf 可以通过重写 fputc() 实现串口打印。
串口中断可以实时接收电脑发送的数据。
WS2812 灯效的本质是不断更新每颗灯的 RGB 数据并刷新显示。


# 2026-08-26

# STM32部分

## 今日目标

基于 `STM32F103C8T6` 和小车原理图，完成 L9110S 电机驱动的 PWM 控制工程，并在 Keil 中形成一个可以烧写运行的项目。

## 工程整理

参考了原来的串口收发打印工程：

```text
C:\Users\18500\Desktop\summer\stm32\2_串口收发打印\2_串口收发打印
```

新工程放在：

```text
C:\Users\18500\Desktop\summer\stm32\PWM
```

尽量复用了原工程中的文件：

- `CORE`
- `SYSTEM`
- `STM32F10x_FWLib`
- `QST_HARDWARE/colorful_led`
- `USER` 基础配置文件

新增了电机模块：

```text
QST_HARDWARE/motor/motor.c
QST_HARDWARE/motor/motor.h
```

## 原理图对应关系

小车使用两颗 `L9110S` 驱动左右两个直流电机。

左电机：

```text
L-IA -> PB7  -> TIM4_CH2 PWM
L-IB -> PB14 -> 方向控制
```

右电机：

```text
R-IB -> PB6  -> TIM4_CH1 PWM
R-IA -> PB13 -> 方向控制
```

编码器引脚也在原理图中出现，但今天主要做开环 PWM 控制，还没有使用编码器闭环。

## PWM 配置

一开始按照课程截图使用：

```c
PWM_Init(7199, 9);
```

频率约为：

```text
72MHz / ((9 + 1) * (7199 + 1)) = 1kHz
```

烧写后电机有明显滴滴声，所以后面改成：

```c
#define PWM_MAX 3599
PWM_Init(PWM_MAX, 0);
```

频率约为：

```text
72MHz / ((0 + 1) * (3599 + 1)) = 20kHz
```

声音比 1kHz 明显更小。

## 调试过程

刚开始烧写后小车没有反应，最后确认原因是板子上的电机供电开关没有打开。这个问题说明：单片机程序运行不代表电机驱动电源已经正常。

之后测试发现：

```c
Set_Pwm(2000, 2000);
```

有时只有一侧轮子能转。提高到：

```c
Set_Pwm(3000, 3000);
```

两边电机都能正常转动。说明这台小车的电机启动 PWM 门槛比较高，低速时容易被静摩擦卡住。

## 串口停车

串口助手发送大写：

```text
HELLO
```

程序收到后会执行：

```c
Motor_Stop();
delay_ms(10000);
```

也就是停车 10 秒，然后继续执行后面的动作。

为了让停车反应更快，主程序没有使用一个很长的 `delay_ms()` 阻塞动作，而是把动作拆成每 `20ms` 检查一次串口命令。

## 当前动作程序

当前程序是一个复杂一点的闭合曲线动作，中间加入了摆头效果。

主要参数：

```c
#define START_PWM         3300
#define BASE_PWM          3000
#define FAST_PWM          3400
#define SLOW_PWM          2600
#define SWING_FAST_PWM    3300
#define SWING_SLOW_PWM    2600
```

动作逻辑：

```text
启动补偿
右弧线
摆头
直行过渡
左大弧线绕回来
摆头
直行过渡
右弧线补回起点附近
```

摆头不是舵机实现的，而是通过左右轮速度差实现：

```c
Run_Motor_For(SWING_FAST_PWM, SWING_SLOW_PWM, 450);
Run_Motor_For(SWING_SLOW_PWM, SWING_FAST_PWM, 450);
```

## 后续调参方向

如果车不动：

```text
优先检查电机供电开关和电池供电。
```

如果一侧轮子不转：

```text
提高该侧 PWM，或先使用 3000 以上的启动补偿。
```

如果曲线太大：

```text
降低慢轮 PWM，例如 2600 -> 2300。
```

如果曲线太小：

```text
提高慢轮 PWM，例如 2600 -> 2800。
```

如果摆头不明显：

```text
降低 SWING_SLOW_PWM，或把 450ms 增大到 600ms。
```

如果摆头太大：

```text
提高 SWING_SLOW_PWM，或缩短摆头时间。
```

# hi3861部分

## 串口双线程输出

在 `1.0Hello_world` 中写了 `hello_world.c`，使用 CMSIS-RTOS2 创建两个线程：

- `thread1`：每 1 秒输出一次 `Hello World!`
- `thread2`：先延时 1 秒，再每 3 秒输出一次 `Hello QST!`

一开始串口助手里中文显示成乱码，例如 `任务1正在运行!` 显示成其他字符。原因是编码不一致：程序输出 UTF-8，串口助手按 GBK/ANSI 解码。解决办法是串口助手切换到 UTF-8，或者程序里只输出英文/ASCII。

## 串口动画实验

在同一目录下新增了几个独立 C 文件：

- `square_loader.c`：每 100 ms 输出一次方形加载动画。
- `progress_bar.c`：每 100 ms 输出进度条和旋转符号。
- `beijing_clock.c`：每秒输出一次东八区软件时钟。

注意：这些文件都带有 `APP_FEATURE_INIT(...)`，通常一次只编译其中一个。需要在当前目录的 `BUILD.gn` 里切换 `sources`。

进度条实验曾经出现链接错误：

```text
undefined reference to `fflush`
```

原因是 Hi3861 LiteOS 当前链接库没有提供 `fflush`。解决办法是删除：

```c
fflush(stdout);
```

## BUILD.gn 常见问题

今天多次遇到 GN 路径和 target 不一致的问题。

如果上级 `applications/sample/wifi-iot/app/BUILD.gn` 写：

```gn
"2.0_SG90:sg90",
```

则必须同时满足：

```text
目录名: applications/sample/wifi-iot/app/2.0_SG90
子目录 BUILD.gn target: static_library("sg90")
源码文件名: 与 sources 中完全一致
```

Linux 区分大小写，所以 `SG90.c` 和 `sg90.c` 不是同一个文件。

如果出现：

```text
Unable to load ".../BUILD.gn"
```

说明 GN 连子目录的 `BUILD.gn` 都没有找到，优先检查目录名是否写错、是否有多余字符、是否实际存在。

如果出现：

```text
Unresolved dependencies
```

说明目录可能存在，但 target 名和上级引用不一致。

## SG90 舵机控制

在 `3.0SG90` 中创建了舵机控制程序。SG90 信号线接 GPIO2，控制原理是输出周期约 20 ms 的脉冲：

```text
0.5 ms  -> 约 0 度/最左侧
1.5 ms  -> 约 90 度/中间
2.5 ms  -> 约 180 度/最右侧
```

当前版本已经改成连续扫动：

- 从最左侧匀速转到最右侧
- 再从最右侧匀速转回最左侧
- 无限循环

核心参数：

```c
#define SG90_LEFT_DUTY_US 500
#define SG90_RIGHT_DUTY_US 2500
#define SG90_DUTY_STEP_US 20
#define SG90_PULSE_REPEAT 2
```

想让舵机转得更快，可以增大 `SG90_DUTY_STEP_US`。想让它转得更慢，可以增大 `SG90_PULSE_REPEAT`。

## 烧录和运行

使用 HiBurn 烧录时，如果出现：

```text
Wait connect success flag (hisilicon) overtime.
```

常见原因是芯片没有进入烧录模式、串口被占用、COM 口选错、USB 线不支持数据传输，或者板子上的开关没有拨回 Hi3861。

烧录常用流程：

```text
关闭串口助手
HiBurn 选择正确 COM 口
加载 Hi3861_wifiiot_app_allinone.bin
按住 BOOT
点击 Burn/Connect
按一下 RESET
开始下载后松开 BOOT
```

烧录完成后，关闭或断开 HiBurn 的串口连接，打开串口助手，然后按一下 `RESET`，程序会重新启动并自动运行。

停止程序的最简单办法是断电、拔 USB，或者按住 `RESET`。如果舵机仍在抖动，也要断开舵机的 5V 电源。


今天整体结论：这辆小车用开环 PWM 可以完成基础运动，但左右电机差异和启动摩擦比较明显。后续如果要走得更准，需要加入编码器闭环控制。

# 2026-8-27

## 一、Hi3861 部分

### 1. 工程路径

```text
C:\Users\18500\Desktop\summer\SSH-192.168.13.128
```

今天主要处理：

```text
4.0Hcsr
```

新增/修改文件：

```text
4.0Hcsr\tick.c
4.0Hcsr\BUILD.gn
```

### 2. tick.c 实现内容

创建了两个软件定时器：

```text
定时器1：每 3 秒测一次超声波距离
定时器2：每 1 秒打印一次当前 tick 值
```

核心功能：

```c
hi_get_tick();   // 获取当前系统 tick
hi_get_us();     // 获取微秒级时间
osTimerNew();    // 创建软件定时器
osTimerStart();  // 启动软件定时器
```

### 3. 超声波接线

按照 QST 鸿蒙小车原理图：

```text
TRIG -> GPIO7 / IO7
ECHO -> GPIO8 / IO8
VCC  -> 5V
GND  -> GND
```

代码中对应：

```c
#define HCSR04_TRIG_GPIO 7
#define HCSR04_ECHO_GPIO 8
```

### 4. 串口输出含义

示例输出：

```text
current tick: 143
current tick: 243
distance timeout: echo never high.
current tick: 346
```

含义：

```text
current tick
表示系统当前 tick 值，会一直增加，说明程序正在运行。

distance timeout: echo never high
表示程序已经发出 TRIG 触发信号，但 ECHO 引脚一直没有检测到高电平。
```

### 5. 当前判断

从串口输出看：

```text
current tick 持续增加
```

说明：

```text
Hi3861 已经在运行
线程创建成功
软件定时器运行正常
串口输出正常
```

但一直出现：

```text
distance timeout: echo never high.
```

说明问题更可能在：

```text
超声波模块供电
TRIG/ECHO 接线
GND 是否共地
ECHO 是否接到 GPIO8
模块本身是否损坏
ECHO 5V 电平与 Hi3861 3.3V IO 是否兼容
```

### 6. HiBurn 烧录问题

曾出现错误：

```text
Wait connect success flag (hisilicon) overtime.
```

含义：

```text
HiBurn 能打开 COM 口，但没有等到 Hi3861 的下载握手回应。
```

注意：

```text
COM3 正常出现，只说明 CH340 USB 转串口芯片正常。
不一定说明 Hi3861 主控正常运行。
```

QST 鸿蒙小车没有 BOOT 键，只有复位键。烧录时应：

```text
HiBurn 选择 COM3
点击烧录
出现 Connecting...
按实体 RESET 键
```

还要确认 USB 串口切换开关拨到 Hi3861 一侧。

---

## 二、Hi3861 硬件排查结论

### 1. 供电问题

排查中发现：

```text
校准后能短暂运行
过几秒后又不行
3861 芯片不上电
```

这说明问题更偏向供电链路。

原理图中 Hi3861 供电路径：

```text
5.0VD -> AMS1117-3.3 -> 3.3VD -> Hi3861 VCC
```

建议排查：

```text
只插 USB，不接外设
测 3.3VD 是否稳定
观察 AMS1117 是否发烫
检查 Type-C 接口、电源开关、拨码开关是否接触不良
```

### 2. ST-Link 与 USB

注意：

```text
ST-Link 不参与 Hi3861 烧录。
```

调试 Hi3861 时建议：

```text
先只接 USB
拔掉 ST-Link
拔掉舵机、超声波、电机等外设
```

避免外设或其它供电路径干扰。

---

## 三、STM32 部分

### 1. 工程路径

```text
C:\Users\18500\Desktop\summer\stm32
```

主要处理过：

```text
PID
TIMER
NFC
```

### 2. PID 工程

PID 工程路径：

```text
C:\Users\18500\Desktop\summer\stm32\PID
```

核心文件：

```text
USER\main.c
USER\stm32f10x_it.c
QST_HARDWARE\SYSTEM_CONTROL\control_system.c
QST_HARDWARE\SYSTEM_CONTROL\control_system.h
QST_HARDWARE\motor\motor.c
QST_HARDWARE\encoder\encoder.c
```

### 3. PID 速度闭环

控制流程：

```text
main.c 初始化编码器、PWM、串口、SysTick
SysTick 每 1ms 中断一次
每 100ms 调用一次 System_Control()
System_Control() 读取编码器速度
PID 计算左右轮 PWM
Set_Pwm() 输出到电机
```

### 4. 串口输出含义

```text
left coder
左轮 100ms 内实际编码器计数

right coder
右轮 100ms 内实际编码器计数

TageA coder
左轮目标编码器计数

TageB coder
右轮目标编码器计数

Motor_A pwm
左轮 PID 输出 PWM

Motor_B pwm
右轮 PID 输出 PWM

State
当前运动状态

Distance
当前阶段累计编码器计数
```

### 5. 运动逻辑

当前实现：

```text
前进约 1m
停车约 0.5s
倒退约 1m
最终停止
```

状态含义：

```text
State = 0  前进
State = 1  暂停 0.5s
State = 2  倒退
State = 3  停止
```

距离控制：

```c
#define DRIVE_DISTANCE_COUNTS 14000
```

暂停时间：

```c
#define DRIVE_PAUSE_TICKS 5
```

因为控制周期是 100ms：

```text
5 * 100ms = 0.5s
```

### 6. 速度参数

前进速度：

```c
#define TARGET_LEFT_RPS       2.2f
#define TARGET_RIGHT_RPS      2.3f
```

倒退速度：

```c
#define BACKWARD_LEFT_RPS     2.7f
#define BACKWARD_RIGHT_RPS    2.8f
```

倒退最小 PWM 补偿：

```c
#define BACKWARD_MIN_PWM      1600
```

作用：

```text
减少倒退时速度过低和抖动问题。
```

### 7. PID 参数

左右轮 PID 参数位于：

```text
control_system.c
```

默认参数：

```c
const float kp = 7.0f;
const float ki = 0.016f;
const float kd = 0.003f;
```

调参原则：

```text
kp 增大：响应更快，但可能抖动
kp 减小：更稳，但响应慢

ki 增大：更容易消除稳态误差
ki 减小：减少积分导致的波动

kd 增大：抑制突变和超调
kd 过大：可能放大编码器噪声
```

---

## 四、彩灯部分

### 1. 文件位置

```text
C:\Users\18500\Desktop\summer\stm32\PID\QST_HARDWARE\colorful_led\colorful_led.c
```

### 2. 修改内容

修改了：

```c
LR_rainbow()
```

实现彩虹跑马灯效果。

效果：

```text
红、黄、绿、青、蓝、紫六色循环移动
左右两边灯同步刷新
```

速度调整：

```c
delay_ms(100);
```

数值越小，跑马灯越快。

---

## 五、今日关键结论

1. `current tick` 持续增加，说明 Hi3861 程序确实在运行。
2. `distance timeout: echo never high` 表示超声波 ECHO 没有被检测到高电平。
3. COM3 正常只说明 CH340 正常，不等于 Hi3861 一定正常。
4. HiBurn 超时通常是下载握手失败，不一定是代码问题。
5. QST 鸿蒙小车没有 BOOT 键，烧录依赖串口切换和 RESET。
6. Hi3861 供电问题很可能存在，重点检查 3.3VD、AMS1117、外设负载和接触问题。
7. STM32 PID 已实现前进、暂停、倒退、停止的闭环运动流程。


# 2026-8-28

# Hi3861部分

今天主要完成了 Hi3861 开发板上的 UART 蓝牙通信、OLED 显示以及 SHT20 温湿度采集实验。

## 1. UART 蓝牙通信实验

工程目录：

```text
applications/sample/wifi-iot/app/5.0Uart
```

本实验使用 UART1 实现串口收发：

```text
GPIO0 -> UART1_TXD
GPIO1 -> UART1_RXD
```

程序中完成了 UART1 初始化、串口接收、消息队列传递以及串口回显功能。手机通过 JDY-16 蓝牙模块发送数据，Hi3861 接收后再通过 UART 回发。

`BUILD.gn` 中的目标名不能写成 `Uart`，否则会和系统自带的 UART 驱动库 `libuart.a` 冲突，导致链接时报 multiple definition 错误。因此工程目标名改为：

```gn
static_library("UartDemo")
```

父级 `app/BUILD.gn` 中应添加：

```gn
"5.0Uart:UartDemo",
```

测试时，串口日志中出现：

```text
UART recv: +CONNECTED
```

说明手机已经连接到 JDY-16 蓝牙模块，并且蓝牙模块能通过 UART 把连接状态发送给 Hi3861。

之后在 BLE 调试 App 中打开 `Notified Values` 下的 `Subscribe`，手机端就能收到 Hi3861 回发的数据。发送 `HELLO` 后，UartAssist 中能看到：

```text
UART recv: HELLO
```

手机端也能收到回显的 `HELLO`，说明蓝牙 UART 收发通信成功。

## 2. OLED 显示实验

工程目录：

```text
applications/sample/wifi-iot/app/7.0OLED
```

OLED 使用 SSD1306 驱动，通过 I2C 总线通信。工程中需要从支持包复制 OLED 驱动文件：

```text
include/hal_bsp_ssd1306.h
include/hal_bsp_ssd1306_fonts.h
include/hal_bsp_ssd1306_bmps.h
src/hal_bsp_ssd1306.c
```

`BUILD.gn` 中需要同时编译主程序和 OLED 驱动：

```gn
sources = [
  "I2c_Ssd1306.c",
  "src/hal_bsp_ssd1306.c",
]
```

并添加头文件路径：

```gn
include_dirs = [
  "//utils/native/lite/include",
  "//kernel/liteos_m/components/cmsis/2.0",
  "//base/iot_hardware/interfaces/kits/wifiiot_lite",
  "include",
]
```

OLED 初始化成功后，串口会输出：

```text
I2C SSD1306 Init is succeeded!!!
```

实验中 OLED 可以显示固定文字和时间信息。

## 3. SHT20 温湿度实验

工程目录：

```text
applications/sample/wifi-iot/app/8.0SHT
```

SHT20 是温湿度传感器，使用 I2C 通信。由于本实验要求同时在 OLED 和 UartAssist 中输出温湿度，因此工程中同时加入了 SHT20 驱动和 SSD1306 OLED 驱动。

需要的文件结构：

```text
8.0SHT
├── BUILD.gn
├── sht.c
├── include
│   ├── hal_bsp_sht20.h
│   ├── hal_bsp_ssd1306.h
│   ├── hal_bsp_ssd1306_fonts.h
│   └── hal_bsp_ssd1306_bmps.h
└── src
    ├── hal_bsp_sht20.c
    └── hal_bsp_ssd1306.c
```

`BUILD.gn` 内容：

```gn
static_library("Sht20") {
  sources = [
    "sht.c",
    "src/hal_bsp_sht20.c",
    "src/hal_bsp_ssd1306.c",
  ]

  include_dirs = [
    "//utils/native/lite/include",
    "//kernel/liteos_m/components/cmsis/2.0",
    "//base/iot_hardware/interfaces/kits/wifiiot_lite",
    "include",
  ]
}
```

父级 `app/BUILD.gn` 中添加：

```gn
"8.0SHT:Sht20",
```

程序中使用信号量控制周期采样，每隔几秒读取一次 SHT20 的温度和湿度。读取到的数据会：

```text
1. 显示到 OLED 屏幕
2. 通过 printf 输出到 UartAssist
```

串口输出格式类似：

```text
temperature = 26.35 C, humidity = 58.42%RH
```

OLED 上显示：

```text
SHT20
Temp: 26.35 C
Humi: 58.42%
```

## 4. 遇到的问题与解决

### UART 库名冲突

最开始 UART 工程中使用：

```gn
static_library("Uart")
```

编译时链接命令中出现两个 `-luart`，导致系统 UART 驱动库被重复链接，出现大量 multiple definition 错误。

解决方法是把应用库名改成：

```gn
static_library("UartDemo")
```

父级 `features` 同步改为：

```gn
"5.0Uart:UartDemo",
```

### OLED 驱动文件路径错误

编译时曾出现找不到：

```text
src/hal_bsp_ssd1306.c
```

原因是 `BUILD.gn` 中写了：

```gn
"src/hal_bsp_ssd1306.c"
```

但实际目录下没有 `src` 文件夹或文件没有放进去。

解决方法是按要求建立：

```text
include
src
```

并把 SSD1306 驱动文件放到对应目录。

### BLE App 接收不到回显

手机连接 JDY-16 后，Hi3861 能收到：

```text
+CONNECTED
```

但手机端一开始收不到回显。原因是 BLE 调试 App 没有打开通知订阅。

解决方法是在 BLE App 中打开：

```text
Notified Values -> Subscribe
```

之后手机就能收到 Hi3861 回传的 `HELLO`。

### 多个实验同时启用导致冲突

SHT20 工程中已经包含 OLED 驱动，如果同时启用 OLED 单独实验，可能会导致 `SSD1306_Init` 等函数重复定义。

因此编译 SHT20 实验时，建议父级 `app/BUILD.gn` 中只保留当前实验：

```gn
features = [
  "8.0SHT:Sht20",
]
```

其他实验先注释掉。

## 5. 今日总结

今天完成了 Hi3861 的 UART、BLE、OLED 和 SHT20 温湿度采集相关实验。重点掌握了 UART1 的 GPIO 复用、BLE 透传模块的连接与通知订阅、OLED 驱动文件的引入方式、SHT20 的 I2C 采集流程，以及 GN 编译配置中目标名和路径必须严格对应的问题。


# STM32部分

今天主要完成了 NFC 读卡控制小车的功能调试，把 NFC 识别、电机前进、停车和灯光控制串了起来。

一开始先对比了 NFC 示例程序和本地代码。示例里直接在串口接收逻辑中判断 `USART2_RX_BUF[19]` 到 `USART2_RX_BUF[22]`，而本地代码把这部分封装成了 `NFC_GetCardAction()` 这样的函数。虽然写法不一样，但本质都是判断 PN532 返回帧中的卡号字段。

PN532 读卡返回的数据格式中，真正用来判断卡号的是第 19 到 22 位：

```c
buf[19]
buf[20]
buf[21]
buf[22]
```

实际测试读出了两张卡：

```text
63:31:47:06
CB:98:A6:05
```

其中 `63:31:47:06` 被设置为停止卡，`CB:98:A6:05` 被设置为前进卡。

前进卡判断逻辑：

```c
if ((buf[19] == 0xCB) && (buf[20] == 0x98) &&
    (buf[21] == 0xA6) && (buf[22] == 0x05)) {
    return NFC_CARD_FORWARD;
}
```

停止卡判断逻辑：

```c
if ((buf[19] == 0x63) && (buf[20] == 0x31) &&
    (buf[21] == 0x47) && (buf[22] == 0x06)) {
    return NFC_CARD_STOP;
}
```

为了让 NFC 工程能够控制电机，把 PWM 工程里的电机驱动模块加入到了 NFC 工程中，包括：

```text
motor.c
motor.h
```

并且在 Keil 工程文件中加入了 `motor.c` 和 motor 头文件路径。

主函数中增加了 PWM 初始化：

```c
PWM_Init(PWM_MAX, 9);
```

这样 TIM4 的 PWM 输出才能正常工作。

当前前进逻辑是：先用最大 PWM 启动一下，再降低到稳定前进速度：

```c
Set_Pwm(FORWARD_START_PWM, FORWARD_START_PWM);
delay_ms(300);
Set_Pwm(FORWARD_PWM, FORWARD_PWM);
```

其中：

```c
#define FORWARD_PWM       6000
#define FORWARD_START_PWM PWM_MAX
```

停止卡扫描后执行：

```c
Motor_Stop();
```

同时停止卡还保留亮灯功能，所以刷停止卡时会停车，并切换灯光状态：

```c
Motor_Stop();

if (led_flag == 0) {
    led_flag = 1;
    R_led_mode();
} else {
    led_flag = 0;
    R_led_CLC();
}
```

调试过程中串口输出能正确显示不同卡号，例如：

```text
00 00 FF 00 FF 00 00 00 FF 0C F4 D5 4B 01 01 00 04 08 04 CB 98 A6 05 C0 00
NFC forward card
```

以及：

```text
00 00 FF 00 FF 00 00 00 FF 0C F4 D5 4B 01 01 00 04 08 04 63 31 47 06 ED 00
NFC stop card
```

这说明 NFC 识别逻辑已经正常进入对应分支。

一开始刷前进卡后电机没有转，但串口已经打印了 `NFC forward card`，说明程序逻辑没有问题。最后确认原因是小车电源开关没有打开。打开电源后，电机控制功能正常。

最终实现效果：

```text
CB:98:A6:05    -> 小车前进
63:31:47:06    -> 小车停止，同时切换灯光
```

今天的关键结论是：NFC 读卡本身只负责识别卡号，真正的动作需要在识别到不同卡号后分配不同的控制逻辑。通过把 NFC 判断结果转换成不同的动作类型，可以让同一个读卡流程控制小车前进、停止和灯光。

# 个人测试

本实验目标：让小车在桌面上自动前进，利用左右两个红外对管检测桌面边缘，避免小车掉下桌子。

## 硬件连接

红外对管检测：

```text
左红外：GPIO14
右红外：GPIO13
```

电机控制：

```text
Hi3861 UART2 TX：GPIO11
Hi3861 UART2 RX：GPIO12
UART2 波特率：115200
```

串口日志通过 `printf` 输出，可以在 UartAssist 中查看运行状态。

## 红外检测逻辑

实测结果：

```text
L=0, R=0：小车在桌面上，安全
L=1, R=0：左侧检测到边缘
L=0, R=1：右侧检测到边缘
L=1, R=1：左右两侧都检测到边缘
```

因此程序判断逻辑为：

```text
只有 L=0 且 R=0 时继续前进
只要 L 或 R 有一边不是 0，就立即停车并执行避边动作
```

## 小车运行流程

当前程序流程：

```text
1. 小车起步
2. 起步阶段使用较高速度 150，持续 300ms
3. 起步后降为巡航速度 120 前进
4. 前进过程中不断读取左右红外对管
5. 只要检测到任意一侧到达边缘，立即停车
6. 短暂停车 100ms
7. 以速度 150 倒车 300ms
8. 根据触发边缘的一侧进行转向
9. 转向完成后继续前进
10. 再次检测到边缘时重复以上过程
```

## 关键参数

程序中的主要可调参数：

```c
#define BRAKE_TIME_MS 100
#define BACKWARD_TIME_MS 300
#define TURN_TIME_MS 450
#define START_SPEED 150
#define CRUISE_SPEED 120
#define START_BOOST_TIME_MS 300
#define BACKWARD_SPEED 150
#define TURN_SPEED 120
```

含义：

```text
BRAKE_TIME_MS：检测到边缘后停车等待时间
BACKWARD_TIME_MS：倒车持续时间
TURN_TIME_MS：转向持续时间
START_SPEED：起步速度
CRUISE_SPEED：正常前进速度
START_BOOST_TIME_MS：起步高速持续时间
BACKWARD_SPEED：倒车速度
TURN_SPEED：转向速度
```

## 调试记录

最开始程序只读取 GPIO7，串口一直显示：

```text
IR status: waiting, no signal
```

后来查项目支持包发现红外对管使用的是 GPIO13 和 GPIO14，不是 GPIO7。GPIO7 和 GPIO8 是超声波模块使用的引脚。

之后改成 GPIO13 / GPIO14 后，使用 `GpioGetInputVal()` 出现：

```text
errno=0x3612
errno=0x3611
```

说明普通 `wifiiot_gpio` 包装接口读取 GPIO13 / GPIO14 不稳定，后来改用底层接口：

```c
hi_gpio_get_input_val(HI_GPIO_IDX_14, &state.left);
hi_gpio_get_input_val(HI_GPIO_IDX_13, &state.right);
```

红外读取成功。

## STM32 配合

小车电机不是 Hi3861 直接控制的，而是：

```text
Hi3861 读取红外并做逻辑判断
Hi3861 通过 UART2 向 STM32 发送电机控制指令
STM32 接收指令后控制电机 PWM
```

Hi3861 发给 STM32 的数据帧格式：

```text
0xFC, 左轮方向, 左轮速度, 右轮方向, 右轮速度, 0xFD
```

例如：

```text
FC 00 78 00 78 FD：前进，左右速度 120
FC 01 96 01 96 FD：倒车，左右速度 150
FC 00 00 00 00 FD：停车
```


# 2026.08.30

## 项目目标

完成 `test2-sg90` 小车避障工程：

- 小车默认前进
- 超声波检测前方障碍物
- 当前避障阈值：`20cm`
- 前方 `<= 20cm` 时停止前进
- 小车原地转向避障
- 避障后继续前进
- STM32 和 Hi3861 代码分别放在不同文件夹

## 工程路径

工程目录：

```text
C:\Users\18500\Desktop\summer\test\test2-sg90
```

主要目录：

```text
Hi3861
STM32
```

## Hi3861 负责内容

Hi3861 主要负责：

- 控制 SG90 舵机
- 读取 HC-SR04 超声波距离
- 判断是否有障碍物
- 通过 UART 给 STM32 发送电机控制指令

主要文件：

```text
C:\Users\18500\Desktop\summer\test\test2-sg90\Hi3861\obstacle_avoidance.c
```

## STM32 负责内容

STM32 主要负责：

- 接收 Hi3861 发来的串口控制帧
- 解析左右电机速度和方向
- 使用 TIM4 PWM 控制 L9110S 电机驱动模块

主要文件：

```text
C:\Users\18500\Desktop\summer\test\test2-sg90\STM32\USER\TIMER.uvprojx
```

截图确认打开的工程路径是正确的：

```text
C:\Users\18500\Desktop\summer\test\test2-sg90\STM32\USER\TIMER.uvprojx
```

## 使用到的引脚

Hi3861：

```text
SG90 舵机信号线：GPIO2
HC-SR04 Trig：GPIO7
HC-SR04 Echo：GPIO8
UART2_TX：GPIO11
UART2_RX：GPIO12
```

STM32：

```text
USART1_RX：PA10
USART1_TX：PA9

USART2_RX：PA3
USART2_TX：PA2

左电机 PWM：PB7 / TIM4_CH2
左电机方向：PB14

右电机 PWM：PB6 / TIM4_CH1
右电机方向：PB13
```

## 串口通信协议

Hi3861 通过 UART2 给 STM32 发送 6 字节控制帧：

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

含义：

```text
0xFC：帧头
left_dir：左电机方向，0 正转，1 反转
left_speed：左电机速度，范围 0~150
right_dir：右电机方向，0 正转，1 反转
right_speed：右电机速度，范围 0~150
0xFD：帧尾
```

STM32 接收到后会把速度值乘以 `20`，再写入 PWM。

## 当前避障逻辑

最终改成了比较简单稳定的逻辑：

1. 上电初始化
2. 先发送停车指令
3. 等待 `200ms`
4. 循环测量前方距离
5. 如果距离有效且 `<= 20cm`
   - 停车
   - 舵机保持中位
   - 小车原地右转
   - 转一小段后停下重新测距
   - 前方距离大于 `20cm` 后退出避障
6. 如果前方距离大于 `20cm`
   - 小车继续前进

## 当前关键参数

```c
#define OBSTACLE_DISTANCE_CM 20.0f
#define FORWARD_SPEED 150
#define TURN_FAST_SPEED 150
#define TURN_SLOW_SPEED (-50)
#define TASK_DELAY_MS 120
#define TURN_CHECK_DELAY_MS 120
#define MAX_TURN_TIME_MS 1800
```

说明：

- `OBSTACLE_DISTANCE_CM`：避障距离阈值
- `FORWARD_SPEED`：前进速度
- `TURN_FAST_SPEED` / `TURN_SLOW_SPEED`：原地转向速度组合
- `TURN_CHECK_DELAY_MS`：每次原地转向持续时间
- `MAX_TURN_TIME_MS`：最多原地转向时间

## 已经修过的问题

### 1. 电机完全不动

原因判断：

- 舵机能动，说明 Hi3861 程序在运行
- 电机不动更可能是 STM32 没收到 Hi3861 的串口帧
- 或者 STM32 工程没有重新烧录
- 或串口接到了 USART2，但代码只监听 USART1

处理：

- STM32 改成 USART1 和 USART2 都能接收控制帧
- 支持 PA10 和 PA3 两个接收口

### 2. 左转右转相反

原因判断：

- 实车电机方向和代码定义方向相反
- 或左右电机接线/安装方向和代码假设不一致

处理：

- 已经把左右转电机速度组合对调

### 3. 开机完全不前进

处理过程：

- 为了避免 STM32 漏掉第一帧，曾加过开机连续发送前进帧
- 后来发现会导致启动时停不下来
- 最终删除开机强制前进
- 改成开机先停车，再测距判断

### 4. 倒车逻辑

原本存在倒车兜底逻辑：

```c
CarBackward();
```

作用是防止原地转向后前方仍然太近，小车卡住。

后来根据需求删除，当前逻辑不再倒车。

## 当前注意事项

烧录时需要注意：

- Hi3861 侧代码要重新编译烧录
- STM32 侧如果改过串口接收，也要重新 Build/Download
- Hi3861 和 STM32 必须共地
- Hi3861 `GPIO11/UART2_TX` 要接到 STM32 `PA10` 或 `PA3`
- 如果电机方向仍然不对，优先调整 `CarTurnRight()` 里的左右电机速度组合

当前重点文件：

```text
C:\Users\18500\Desktop\summer\test\test2-sg90\Hi3861\obstacle_avoidance.c
C:\Users\18500\Desktop\summer\test\test2-sg90\STM32\SYSTEM\usart\usart.c
C:\Users\18500\Desktop\summer\test\test2-sg90\STM32\USER\main.c
```
STM32 端需要烧录支持该协议的程序，否则 Hi3861 串口显示动作正常，但小车不会动。

# 8.29
OLED显示和SHT20温湿度实验丢失文件上传完成，感谢邮件提醒。


# 8.31

## AP3216C 光照控制 LED 实验记录

今天完成了 `9.0AP3216` 工程的 AP3216C 光照传感器实验，实现了光照采集、串口输出、OLED 显示，以及根据光照强度控制车上 LED 灯亮灭。

## 实现功能

本次工程主要实现以下功能：

- 使用 AP3216C 传感器采集光照强度
- 通过串口输出 AP3216C 采集到的数据
- 通过 OLED 显示光照数据和 LED 状态
- 根据光照强度控制 LED：
  - 遮光或光照较弱时，LED 亮白灯
  - 有光照时，LED 熄灭

## Hi3861 部分

Hi3861 负责读取 AP3216C 传感器数据，并根据光照值判断 LED 状态。

主要使用的文件包括：

```text
9.0AP3216\ap.c
9.0AP3216\include\hal_bsp_ap3216c.h
9.0AP3216\src\hal_bsp_ap3216c.c
9.0AP3216\src\hal_bsp_ssd1306.c
```

AP3216C 通过 I2C 与 Hi3861 通信：

```text
I2C0 SDA: GPIO10
I2C0 SCL: GPIO9
AP3216C 地址: 0x3C
```

程序运行后，串口会输出类似信息：

```text
AP3216C ir=4, als=338, ps=151, led=OFF
AP3216C ir=1, als=6, ps=229, led=ON
```

其中：

- `ir` 表示红外数据
- `als` 表示环境光照强度
- `ps` 表示接近传感器数据
- `led` 表示当前 LED 状态

本次使用 `als` 作为判断依据：

```text
als <= 50: LED ON
als > 50: LED OFF
```

## OLED 显示

OLED 用于实时显示 AP3216C 数据和 LED 状态。

显示内容包括：

```text
AP3216C
ALS:xxx
IR:xxx PS:xxx
LED:ON/OFF
```

这样可以不用一直看串口，也能直接从 OLED 上观察当前光照值和 LED 判断结果。

## STM32 部分

车上的 LED 灯由 STM32 控制，因此 Hi3861 在判断光照状态后，通过 UART 向 STM32 发送控制命令。

STM32 工程路径为：

```text
9.0AP3216\led
```

主要文件：

```text
9.0AP3216\led\USER\main.c
9.0AP3216\led\SYSTEM\usart\usart.c
9.0AP3216\led\SYSTEM\usart\usart.h
```

串口配置：

```text
USART1
波特率: 9600
TX: PA9
RX: PA10
```

Hi3861 发送命令：

```text
L1: 点亮车上左右两侧 WS2812 LED，显示白色
L0: 熄灭车上左右两侧 WS2812 LED
```

STM32 接收到命令后，根据命令控制车灯：

- 收到 `L1`：全部 LED 亮白灯
- 收到 `L0`：全部 LED 熄灭

## 问题记录

调试过程中遇到 AP3216C 初始化失败的问题，串口输出：

```text
AP3216C reset failed, status=0x80001182
AP3216C init failed
```

这个错误表示 Hi3861 通过 I2C 写 AP3216C 复位寄存器时失败，AP3216C 没有应答。

排查后发现不是代码逻辑问题，而是硬件连接问题。重新插拔数据线后，AP3216C 可以正常输出数据。

正常输出示例：

```text
AP3216C ir=4, als=338, ps=151, led=OFF
AP3216C ir=1, als=6, ps=229, led=ON
```

## 代码整理

后续对 STM32 的 `main.c` 做了整理，删除了与本实验无关的内容，包括：

- 电机控制相关代码
- 编码器初始化
- PWM 输出
- `Motor_Stop()`
- `Set_Pwm()`

整理后，STM32 端只保留串口接收和 LED 控制逻辑，使工程更清晰。


# 传感器应用

## 今日内容

完成了 `10.0SUM` 智能小车综合工程的整合和调试。

本工程将前面几个独立实验功能合并到同一个项目中，包括红外防跌落、超声波避障、SG90 舵机扫描、OLED 显示、SHT20 温湿度采集、AP3216C 三合一传感器采集、蓝牙串口通信、电机控制以及 LED 控制。

## Hi3861 部分

Hi3861 作为主控，负责传感器读取、避障判断和向 STM32 下发控制命令。

主要文件：

```text
10.0SUM/sum.c
10.0SUM/BUILD.gn
10.0SUM/include/hal_bsp_sht20.h
10.0SUM/src/hal_bsp_sht20.c
10.0SUM/src/hal_bsp_ap3216c.c
10.0SUM/src/hal_bsp_ssd1306.c
```

## STM32 部分

STM32 作为执行端，负责接收 Hi3861 发来的串口命令，并控制电机和车灯。

主要文件：

```text
10.0SUM/TIMER/USER/main.c
10.0SUM/TIMER/main.c
10.0SUM/TIMER/SYSTEM/usart/usart.c
10.0SUM/TIMER/SYSTEM/usart/usart.h
```

STM32 串口波特率统一为：

```text
115200
```

## 功能实现

### 1. 红外防跌落

使用左右两个红外对管检测桌面边缘。

引脚：

```text
GPIO14：左侧红外
GPIO13：右侧红外
```

当前边缘触发电平：

```c
#define IR_EDGE_VALUE HI_GPIO_VALUE1
```

如果桌面和边缘判断反了，只需要改这一行：

```c
#define IR_EDGE_VALUE HI_GPIO_VALUE0
```

检测到边缘后，小车会执行：

```text
刹车
后退
向安全方向转弯
继续前进
```

为了让刹车更及时，将采样周期缩短为：

```c
#define SAMPLE_PERIOD_MS 30
```

### 2. 超声波避障

使用 HC-SR04 检测前方障碍物。

引脚：

```text
GPIO7：Trig
GPIO8：Echo
```

避障距离阈值：

```c
#define OBSTACLE_DISTANCE_CM 20.0f
```

当前逻辑：

```text
检测到前方障碍
停车
舵机左转测距
舵机右转测距
回中
后退
选择空间较大的方向转弯
再次检测前方距离
```

### 3. SG90 舵机

舵机信号引脚：

```text
GPIO2
```

当前角度 PWM：

```c
#define SG90_LEFT_DUTY_US 2400
#define SG90_CENTER_DUTY_US 1650
#define SG90_RIGHT_DUTY_US 900
```

调试时发现舵机左右方向反了，因此已经将左右 PWM 对调。

### 4. OLED 显示

OLED 用于显示 SHT20 和 AP3216C 的数据。

I2C 引脚：

```text
GPIO9：I2C0 SCL
GPIO10：I2C0 SDA
```

显示内容：

```text
SHT20 温度
SHT20 湿度
AP3216C IR
AP3216C ALS
AP3216C PS
```

OLED 每秒刷新一次。

### 5. SHT20 温湿度检测

SHT20 用于检测温度和湿度。

显示格式类似：

```text
T:26.4C H:48.0%
```

串口打印格式类似：

```text
Task 2 running: OLED update, temp=26.4C humi=48.0% IR=11 ALS=429 PS=149 LED=OFF
```

### 6. AP3216C 数据检测

AP3216C 读取三个值：

```text
IR：红外数据
ALS：环境光数据
PS：接近距离数据
```

其中 ALS 用于判断 LED 是否点亮。

### 7. LED 控制

LED 控制参考了 `9.0AP3216` 工程。

Hi3861 本地 GPIO6 LED 逻辑：

```text
GPIO6
高电平：灭
低电平：亮
```

当环境光较暗时点亮：

```c
#define ALS_DARK_THRESHOLD 50
```

判断逻辑：

```text
ALS <= 50：LED ON
ALS > 50：LED OFF
```

如果使用的是 STM32 车灯，则 Hi3861 会通过 UART2 给 STM32 发送：

```text
L1：开灯
L0：关灯
```

STM32 端已经兼容该命令。

### 8. 蓝牙通信

蓝牙使用 Hi3861 UART1。

引脚：

```text
GPIO0：UART1 TX
GPIO1：UART1 RX
```

波特率：

```text
9600
```

当前支持简单命令：

```text
A：自动模式
S：停止
F：前进
B：后退
L：左转
R：右转
```

如果 UART1 初始化失败，可能是 UART1 被调试串口或系统日志占用。

### 9. 电机控制

Hi3861 通过 UART2 向 STM32 发送电机控制帧。

Hi3861 引脚：

```text
GPIO11：UART2 TX
GPIO12：UART2 RX
```

波特率：

```text
115200
```

电机帧格式：

```text
0xFC, left_dir, left_speed, right_dir, right_speed, 0xFD
```

方向：

```text
0：正转
1：反转
```

速度范围：

```text
0 到 150
```

STM32 收到后转换为 PWM：

```c
Set_Pwm(left_speed * 20, right_speed * 20);
```

## 串口任务打印

程序会通过串口打印当前任务运行状态。

示例：

```text
10.0SUM project start.
Task 1 running: car safety: IR edge protection + ultrasonic obstacle avoidance
Task 2 running: OLED display: SHT20 temperature/humidity + AP3216C IR/ALS/PS
Task 3 running: Bluetooth UART1 communication
```

运行过程中还会打印：

```text
Task 1 running: car safety, IR L=... R=... distance=... cm mode=...
Task 2 running: OLED update, temp=... humi=... IR=... ALS=... PS=... LED=...
Task 3 running: Bluetooth recv: ...
```

## 调试记录

### 旧程序未更新问题

一开始串口仍然输出：

```text
AP3216C ir=..., als=..., ps=..., led=OFF
```

说明当时板子运行的还是 `9.0AP3216` 的旧程序，不是 `10.0SUM`。

后来将 `BUILD.gn` 中目标名改为：

```text
SumCar
```

最终应编译烧录：

```text
10.0SUM:SumCar
```

### 红外判断反向问题

调试中发现桌面和边缘检测反了，因此通过修改：

```c
#define IR_EDGE_VALUE HI_GPIO_VALUE1
```

来调整边缘触发电平。

### 刹车过晚问题

原先红外检测会受到超声波测距阻塞影响，导致边缘刹车偏晚。

后来调整为：

```text
先读取红外
先判断边缘
如果安全，再进行超声波测距
```

并缩短采样周期，提高响应速度。

### 舵机方向反向问题

调试发现舵机左右方向反了，因此对调：

```c
#define SG90_LEFT_DUTY_US 2400
#define SG90_RIGHT_DUTY_US 900
```

### LED 不亮问题

LED 不亮的原因可能不是 Hi3861 GPIO6，而是使用的是 STM32 侧车灯。

因此 STM32 端增加了对 `L1 / L0` 命令的解析，同时保留原有电机帧解析。

现在 STM32 串口同时支持：

```text
0xFC ... 0xFD：电机控制
L1：LED 开
L0：LED 关
```

## 当前注意事项

烧录时需要同时更新：

```text
Hi3861：10.0SUM:SumCar
STM32：10.0SUM/TIMER/USER/main.c 所在 Keil 工程
```

如果只烧录 Hi3861，不更新 STM32，电机或车灯功能可能不会正常工作。

如果 LED 仍不亮，需要确认当前使用的是：

```text
Hi3861 GPIO6 LED
```

还是：

```text
STM32 WS2812 车灯
```

两者的控制方式不同。


# 9.1

## 今日工作内容

今天主要完成了小车数据上云、网页实时显示，以及上报延迟问题的排查和修正。

## 华为云 MQTT 上报

小车已经成功连接 WiFi，并通过 MQTT 将实时数据上传到华为云 IoTDA。

上报内容包括：

```text
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

串口出现以下日志时，说明 MQTT 连接和数据上报成功：

```text
Task 4 running: Huawei Cloud MQTT connected
Task 4 running: Huawei Cloud data upload OK
```

过程中遇到过 `MQTT CONNACK failed, code=4`，原因是 MQTT clientId 中的时间戳校验导致密码过期。后来改用不校验时间戳的 clientId：

```text
6a9643457f2e6c302f94fcf9_qstcar_0_0_2026090104
```

避免了长时间运行后因时间戳失效导致连接失败。

## 网页实时数据展示

在目录：

```text
C:\Users\18500\Desktop\summer\test\test3-html
```

完成了一个基于 Vue 和 Java 的网页后端工程。

主要文件：

```text
car.html
CarCloudServer.java
pom.xml
README_WEB.md
```

网页功能包括：

```text
显示小车实时数据
绘制温湿度折线图
绘制 AP3216C 光照、红外、接近值折线图
绘制超声波距离折线图
显示红外边界和 LED 状态
显示 Cloud Event、Cloud Age、AMQP Count、AMQP Delta
```

页面地址：

```text
http://localhost:8080/car.html
```

## 华为云数据读取方式调整

最开始后端通过 IoTDA 设备影子接口读取数据：

```text
/v5/iot/{project_id}/devices/{device_id}/shadow
```

该方式能够读到云端数据，但延迟较大，`Cloud Event` 经常几十秒才变化一次，不适合实时折线图。

后来改为使用华为云 IoTDA 的 AMQP 数据转发队列。

AMQP 配置：

```text
host: 3c95083845.st1.iotda-app.cn-north-4.myhuaweicloud.com
port: 5671
queue: qst_queue
instance_id: 5357e9ef-bcdf-4934-9c94-2924c34fde2b
```

AMQP 接入凭证：

```text
access_key: JpadGfUK
access_code: hDDms2ZYfrMfXvRpqgfUW2tJqnSUya8D
```

Java 后端使用 Maven 和 Apache Qpid JMS 连接 AMQP 队列。连接成功时日志为：

```text
connected to server: amqps://3c95083845.st1.iotda-app.cn-north-4.myhuaweicloud.com:5671
```

## AMQP 延迟问题排查

网页中加入了诊断字段：

```text
AMQP Count
AMQP Delta
Cloud Age
```

用于判断数据延迟来自哪里。

一开始 AMQP Delta 约为：

```text
50 s
```

说明 Java 后端已经连接队列，但队列大约 50 秒才收到一条新消息。

继续查看小车串口后发现：

```text
Task 4 running: Huawei Cloud data upload OK
```

本身也是约 50 秒打印一次，因此问题不在网页和 AMQP，而在小车端延时逻辑。

## osDelay Tick 问题修正

原代码中使用：

```c
osDelay(5000);
```

期望延时 5000 ms，也就是 5 秒。

但在当前 Hi3861/CMSIS-RTOS 环境中，`osDelay()` 的参数是 OS tick，不是毫秒。当前 1 tick 约为 10 ms，所以：

```text
osDelay(5000) 实际约等于 50 秒
```

这导致：

```text
Task4 云端上报 5 秒变成约 50 秒
Task2 OLED 1 秒更新变成约 10 秒
Task1 安全任务循环也被放慢
```

为修正该问题，在 `sum.c` 中加入：

```c
#define OS_TICK_MS 10

static void DelayMs(uint32_t ms)
{
    uint32_t ticks = (ms + OS_TICK_MS - 1) / OS_TICK_MS;
    osDelay(ticks == 0 ? 1 : ticks);
}
```

并将所有 `osDelay(...)` 替换为：

```c
DelayMs(...)
```

之后将云端上报周期改为：

```c
#define CLOUD_UPLOAD_PERIOD_MS 2000
```

即 2 秒上报一次。

## 边界抽动问题修正

修正 `osDelay` 后，所有运动动作恢复真实毫秒语义，导致原先边界避让参数过短。

原参数：

```c
#define BACKWARD_TIME_MS 80
#define EDGE_TURN_TIME_MS 180
```

修正 tick 后，实际就只有 80 ms 和 180 ms，小车还没离开桌沿就恢复判断，导致在边界处反复刹车、后退、转向，表现为抽动。

因此调整为：

```c
#define BACKWARD_TIME_MS 350
#define EDGE_TURN_TIME_MS 350
#define EDGE_RELEASE_TIME_MS 250
```

并加入连续安全检测逻辑：红外传感器必须连续检测到安全状态 250 ms 后，才允许恢复前进。

## 当前状态

目前已经完成：

```text
小车 WiFi 连接
华为云 MQTT 上报
AMQP 数据转发接入
Java 后端读取 AMQP 队列
Vue 网页实时折线图展示
2 秒云端上报周期配置
osDelay tick 问题修正
边界抽动问题初步修正
```

后续实车验证重点：

```text
确认 Task4 是否约 2 秒打印一次 data upload OK
确认网页 AMQP Delta 是否降到约 2 秒
确认边界避让是否稳定
确认超声波避障动作是否因为真实毫秒延时变短
```

如果超声波避障转向不足，可继续调整：

```c
#define OBSTACLE_BACKWARD_TIME_MS 250
#define OBSTACLE_TURN_TIME_MS 500
#define MAX_OBSTACLE_TURN_TIME_MS 1500
```


# 9.2

## 项目目标

使用手机蓝牙调试工具控制小车运动。手机通过蓝牙模块连接 Hi3861，Hi3861 接收控制指令后，再控制 STM32，最终由 STM32 驱动小车电机。

整体控制链路：

```text
手机 LightBlue
  -> 蓝牙模块
  -> Hi3861
  -> STM32
  -> 电机
```

## 手机端控制协议

手机端发送单个 ASCII 字符控制小车：

```text
O：停止
W：前进
A：左转
D：右转
S：后退
I：低速前进
K：高速前进
```

## 最初遇到的问题

一开始烧录后，串口打印的是之前综合实验的日志：

```text
10.0SUM project start.
Task 1 running: car safety...
Task 2 running: OLED...
Task 3 running: WiFi connect
Task 4 running: Huawei Cloud MQTT...
```

说明板子里运行的不是蓝牙控制程序，而是之前的 `10.0SUM` 程序。

原因是 `BUILD.gn` 没有保存，构建时仍然链接了旧模块。

修正后，构建配置中启用：

```gn
"11.0_bluetooth:bluetooth"
```

重新编译烧录后，启动日志变为：

```text
UART1 example start.
```

说明已经烧录到蓝牙 UART 示例程序。

## BUILD.gn 配置

应用层 `BUILD.gn` 中只保留蓝牙工程：

```gn
lite_component("app") {
    features = [
        "11.0_bluetooth:bluetooth",
    ]
}
```

蓝牙模块目录下的 `BUILD.gn`：

```gn
static_library("bluetooth") {
    sources = [
        "bluetooth.c",
    ]

    include_dirs = [
        "//utils/native/lite/include",
        "//kernel/liteos_m/components/cmsis/2.0",
        "//base/iot_hardware/interfaces/kits/wifiiot_lite",
    ]
}
```

编译日志中应当出现：

```text
-lbluetooth
```

而不应该再出现旧工程的：

```text
-lsum
```

## 蓝牙接收验证

烧录蓝牙程序后，手机发送指令，串口能看到：

```text
UART recv: W
UART recv: O
```

说明：

```text
手机 -> 蓝牙模块 -> Hi3861 UART1
```

这条链路是通的。

但是此时小车不动，因为程序还只是 UART 回显，没有把手机命令转发给 STM32。

## Hi3861 与 STM32 的通信方式

原来的小车控制方案中，Hi3861 不是直接控制电机，而是通过 UART 给 STM32 发送电机控制帧。

电机控制帧格式：

```text
0xFC  左轮方向  左轮速度  右轮方向  右轮速度  0xFD
```

方向定义：

```text
0：正转
1：反转
```

例如前进：

```text
FC 00 96 00 96 FD
```

例如停止：

```text
FC 00 00 00 00 FD
```

## UART1 和 UART2 冲突问题

原计划是：

```text
UART1：连接蓝牙模块
UART2：连接 STM32
```

但是实际测试发现，Hi3861 的 SDK 中 `UartInit()` 不能稳定同时初始化 UART1 和 UART2。

先初始化 UART1，再初始化 UART2，会出现：

```text
Failed to init motor UART2, err code: 4294967295
```

先初始化 UART2，再初始化 UART1，会出现：

```text
Failed to init UART1, err code: 4294967295
```

`4294967295` 实际就是 `-1`，表示初始化失败。

因此不能直接使用两个硬件 UART 完成转发。

## 最终采用的方案

最终保留 UART1 给蓝牙模块使用，Hi3861 通过 GPIO11 软件模拟 UART TX，将控制帧发送给 STM32。

最终链路：

```text
手机 LightBlue
  -> 蓝牙模块
  -> Hi3861 UART1(GPIO0/GPIO1)
  -> Hi3861 GPIO11 软件串口
  -> STM32 USART1
  -> STM32 PWM 控制电机
```

## Hi3861 端实现

蓝牙模块连接 Hi3861 UART1：

```text
GPIO0：UART1_TX
GPIO1：UART1_RX
波特率：9600
```

初始化 UART1：

```c
IoSetFunc(WIFI_IOT_IO_NAME_GPIO_0, WIFI_IOT_IO_FUNC_GPIO_0_UART1_TXD);
IoSetFunc(WIFI_IOT_IO_NAME_GPIO_1, WIFI_IOT_IO_FUNC_GPIO_1_UART1_RXD);
UartInit(WIFI_IOT_UART_IDX_1, &uartAttr, NULL);
```

GPIO11 用作软件串口 TX：

```c
IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_GPIO);
GpioSetDir(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_GPIO_DIR_OUT);
GpioSetOutputVal(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_GPIO_VALUE1);
```

软件串口波特率为 9600：

```c
#define SOFT_UART_BIT_US 104
```

因为：

```text
1 / 9600 ≈ 104us
```

发送一个字节时：

```text
起始位：GPIO11 拉低
数据位：低位先发，共 8 位
停止位：GPIO11 拉高
```

## Hi3861 命令解析

Hi3861 收到手机发送的字符后，根据指令调用不同动作：

```text
O -> 停止
W -> 前进
A -> 左转
D -> 右转
S -> 后退
I -> 低速前进
K -> 高速前进
```

换行符会被忽略：

```c
case '\r':
case '\n':
    return;
```

## 小车动作参数

最终使用的速度参数：

```text
O：  0,    0
W：150,  150
A：-50,  150
D：150,  -50
S：-150, -150
I：100,  100
K：150,  150
```

其中左右两个数分别代表左轮速度和右轮速度。负数表示反转。

## 稳定性优化

由于 GPIO 软件串口对时序要求较高，为了提升可靠性，做了几项优化。

蓝牙读取间隔从 200ms 降到 10ms：

```c
#define UART_READ_IDLE_US (10 * 1000)
```

发送软件串口帧时锁住调度，避免发送过程中被任务切走：

```c
osKernelLock();
发送数据帧
osKernelRestoreLock();
```

普通运动命令重复发送 3 次：

```c
#define MOTOR_FRAME_REPEAT 3
```

停止命令重复发送 8 次：

```c
#define MOTOR_STOP_FRAME_REPEAT 8
```

这样即使某一帧没被 STM32 正确接收，后面的重复帧也能提高成功率。停止命令重复次数更多，是为了让小车能尽快停下来。

## STM32 端实现

STM32 负责真正控制电机。

STM32 USART1 接收 Hi3861 发来的数据：

```text
PA9：USART1_TX
PA10：USART1_RX
波特率：9600
```

初始化：

```c
uart_init(9600);
```

STM32 接收到完整 6 字节帧后解析：

```c
0xFC  left_dir  left_speed  right_dir  right_speed  0xFD
```

如果方向位不为 0，则对应电机速度取负：

```c
if (motor_frame[1] != 0) {
    left_speed = -left_speed;
}

if (motor_frame[3] != 0) {
    right_speed = -right_speed;
}
```

最后输出 PWM：

```c
Set_Pwm(left_speed * 20, right_speed * 20);
```

这里乘以 20，是因为手机和 Hi3861 传递的是 `0~150` 的速度值，而 STM32 的 PWM 范围更大，需要放大后才能让电机正常转动。

## STM32 单字符协议

STM32 端也支持直接接收手机协议字符：

```text
O：停止
W：前进
A：左转
D：右转
S：后退
I：低速前进
K：高速前进
```

对应函数：

```c
O -> car_stop()
W -> car_forward()
A -> car_left()
D -> car_right()
S -> car_backward()
I -> stm32motor_control(100, 100)
K -> stm32motor_control(150, 150)
```

后来发现 STM32 单字符控制路径和帧控制路径速度单位不一致。帧控制会执行：

```c
Set_Pwm(left_speed * 20, right_speed * 20);
```

而单字符路径一开始没有乘 20，导致 `W/I/K` 正向速度太小，电机不明显运动。

最终修正为：

```c
void stm32motor_control(int left_motor, int right_motor)
{
    Set_Pwm(left_motor * 20, right_motor * 20);
}
```

## 调试过程记录

一开始 LightBlue 能发送命令，串口能打印：

```text
UART recv: W
UART recv: O
```

但小车不动。原因是 Hi3861 没有转发命令给 STM32。

后来加入 UART2 转发后，出现：

```text
Failed to init motor UART2
```

说明 UART2 初始化失败。

调整初始化顺序后，又出现：

```text
Failed to init UART1
```

说明 UART1 和 UART2 不能同时使用。

于是改为 GPIO11 软件串口。

之后出现：

```text
A、D、O 有反应
W、S、I、K 不稳定或无反应
```

原因包括：

```text
软件串口时序不稳定
STM32 速度缩放不一致
前进速度参数偏小
```

最终通过以下方式改善：

```text
软件串口发送时锁调度
命令重复发送
停止命令重复更多次
STM32 速度统一乘 20
前进速度提高到 150
```

## 最终效果

启动后串口应打印：

```text
Bluetooth car control start.
```

手机发送指令后应打印：

```text
UART recv: W
Bluetooth command: W
```

小车可以通过手机控制：

```text
W：前进
A：左转
D：右转
S：后退
O：停止
I：低速前进
K：高速前进
```

## 注意事项

Hi3861 和 STM32 必须共地。

Hi3861 GPIO11 要接到 STM32 的串口 RX。

STM32 串口波特率必须和 Hi3861 软件串口一致，目前为 9600。

手机端发送普通 ASCII 字符即可，不需要发送十六进制。

如果 LightBlue 自动附带 `\r` 或 `\n`，程序会忽略换行，不影响控制。

如果后续要进一步提高可靠性，可以考虑：

```text
使用 I2C 替代软件 UART
尝试 Hi3861 底层 hi_uart 同时打开 UART1 和 UART2
让蓝牙模块直接连接 STM32
```

当前方案是在不改变“手机连接 Hi3861，再控制 STM32”这个结构的前提下，实现的可用版本。
