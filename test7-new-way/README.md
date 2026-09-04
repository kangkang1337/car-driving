# test7-new-way

巡线实验工程。黑线应位于左右两个红外传感器中间，地板返回 `0`，黑线返回 `1`。

## Hi3861

- 主程序：`way.c`
- 构建目标：`static_library("way")`
- 父级 `applications/sample/wifi-iot/app/BUILD.gn` 中启用：

```gn
"test7-newway:way",
```

已移除 WiFi、蓝牙、云端、OLED、SHT20、AP3216C、超声波和舵机依赖。

引脚：

- 左红外：GPIO13
- 右红外：GPIO14
- 到 STM32：UART2，GPIO11 TX，GPIO12 RX，115200

判断：

- `0,0`：黑线在两传感器中间，直行
- `1,0`：左传感器压到黑线，立即向左短修正
- `0,1`：右传感器压到黑线，立即向右短修正
- `1,1`：横向黑线/岔路口标记

终点按“黑-白-黑”判定：先检测到一根横向黑线，离开后进入中间白线区域，再在短时间内检测到第二根横向黑线，才判定为终点。检测到终点后，小车立即停车并发送闪灯命令。

主循环每 `10ms` 轮询一次红外状态。修正不是固定长时间转向，而是每次按最新红外值发一帧短修正命令，下一轮马上重新判断，避免压线后继续转过头。

如果双黑不是终点，就按岔路口处理。第 `1` 次双黑向左偏，第 `2` 次双黑向右偏，后续按奇偶交替。偏向动作结束后继续进入 `10ms` 轮询巡线。岔路偏向时间已加长，方便在直岔口真正转过去。

## STM32

Keil 工程：`TIMER/USER/TIMER.uvprojx`

保留模块：

- USART1 接收 Hi3861 控制帧
- TIM4 PWM 电机控制
- WS2812 车灯控制

已移除 PID/编码器工程引用。收到 `E\n` 后停车并循环闪烁所有车灯。

## 调参入口

`way.c` 中优先调整这些值：

- `CRUISE_SPEED`
- `CORRECT_FAST_SPEED`
- `CORRECT_SLOW_SPEED`
- `JUNCTION_BIAS_MS`
- `JUNCTION_LEFT_FAST_SPEED`
- `JUNCTION_LEFT_SLOW_SPEED`
- `JUNCTION_RIGHT_FAST_SPEED`
- `JUNCTION_RIGHT_SLOW_SPEED`
- `SAMPLE_PERIOD_MS`
- `MARKER_RELEASE_SPEED`
- `FINISH_DETECT_SPEED`
- `FINISH_FIRST_RELEASE_TIMEOUT_MS`
- `FINISH_WHITE_GAP_MAX_MS`
- `FINISH_SECOND_CONFIRM_MS`

如果左右修正方向和实车相反，交换 `CarCorrectLeft()` 和 `CarCorrectRight()` 中的左右速度即可。

# 9.4
进行了大量数据和程序的精校，如放弃原有岔路口判定并更改为按双黑的次数进行左转或右转的执行
最终到达终点
