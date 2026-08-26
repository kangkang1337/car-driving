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


# 2026-08-26 PWM 电机控制实验记录

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
