#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "hal_bsp_ap3216c.h"
#include "hal_bsp_sht20.h"
#include "hal_bsp_ssd1306.h"
#include "hi_errno.h"
#include "hi_gpio.h"
#include "hi_time.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"

#define MOTOR_UART WIFI_IOT_UART_IDX_2
#define MOTOR_BAUD_RATE 115200

#define BT_UART WIFI_IOT_UART_IDX_1
#define BT_BAUD_RATE 9600
#define BT_BUFFER_SIZE 128
#define BT_READ_IDLE_MS 50

#define CAR_LED_GPIO WIFI_IOT_IO_NAME_GPIO_6
#define CAR_LED_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_6_GPIO
#define ALS_DARK_THRESHOLD 50

#define IR_LEFT_IO_NAME WIFI_IOT_IO_NAME_GPIO_14
#define IR_RIGHT_IO_NAME WIFI_IOT_IO_NAME_GPIO_13
#define IR_LEFT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_14_GPIO
#define IR_RIGHT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_13_GPIO
#define IR_EDGE_VALUE HI_GPIO_VALUE1

#define SG90_GPIO 2
#define SG90_LEFT_DUTY_US 2400
#define SG90_CENTER_DUTY_US 1650
#define SG90_RIGHT_DUTY_US 900
#define SG90_PWM_PERIOD_US 20000
#define SG90_HOLD_PULSES 18

#define HCSR04_TRIG_GPIO 7
#define HCSR04_ECHO_GPIO 8
#define HCSR04_ECHO_TIMEOUT_US 30000
#define OBSTACLE_DISTANCE_CM 20.0f
#define INVALID_DISTANCE_CM (-1.0f)

#define START_SPEED 150
#define CRUISE_SPEED 120
#define BACKWARD_SPEED 150
#define TURN_SPEED 130
#define TURN_FAST_SPEED 150
#define TURN_SLOW_SPEED (-50)

#define SAMPLE_PERIOD_MS 30
#define BRAKE_TIME_MS 300
#define BACKWARD_TIME_MS 50
#define EDGE_TURN_TIME_MS 180
#define START_BOOST_TIME_MS 300
#define TURN_CHECK_DELAY_MS 120
#define OBSTACLE_BACKWARD_TIME_MS 50
#define OBSTACLE_TURN_TIME_MS 300
#define MAX_OBSTACLE_TURN_TIME_MS 900

#define CAR_TASK_STACK_SIZE (1024 * 6)
#define SENSOR_TASK_STACK_SIZE (1024 * 6)
#define BT_TASK_STACK_SIZE (1024 * 4)
#define TASK_PRIO 25
#define OLED_TEXT_SIZE 16

typedef enum {
    CAR_STATE_FORWARD = 0,
    CAR_STATE_BRAKE,
    CAR_STATE_BACKWARD,
    CAR_STATE_EDGE_TURN,
} CarState;

typedef enum {
    TURN_RIGHT = 0,
    TURN_LEFT,
} TurnDirection;

typedef enum {
    BT_MODE_AUTO = 0,
    BT_MODE_STOP,
    BT_MODE_FORWARD,
    BT_MODE_BACKWARD,
    BT_MODE_LEFT,
    BT_MODE_RIGHT,
} BtMode;

typedef struct {
    hi_gpio_value left;
    hi_gpio_value right;
    unsigned int leftRet;
    unsigned int rightRet;
} IrState;

static volatile BtMode g_btMode = BT_MODE_AUTO;
static osMutexId_t g_motorUartMutex = NULL;

static void CarLedInit(void)
{
    IoSetFunc(CAR_LED_GPIO, CAR_LED_IO_FUNC);
    GpioSetDir(CAR_LED_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetOutputVal(CAR_LED_GPIO, WIFI_IOT_GPIO_VALUE1);
}

static void CarLedSetByLight(uint16_t als)
{
    WifiIotGpioValue value = (als <= ALS_DARK_THRESHOLD) ? WIFI_IOT_GPIO_VALUE0 : WIFI_IOT_GPIO_VALUE1;
    GpioSetOutputVal(CAR_LED_GPIO, value);
}

static void PrintTaskRun(unsigned int taskNo, const char *content)
{
    printf("Task %u running: %s\r\n", taskNo, content);
}

static void MotorUartWrite(const unsigned char *data, unsigned int len)
{
    if (g_motorUartMutex != NULL) {
        (void)osMutexAcquire(g_motorUartMutex, osWaitForever);
    }

    (void)UartWrite(MOTOR_UART, data, len);

    if (g_motorUartMutex != NULL) {
        (void)osMutexRelease(g_motorUartMutex);
    }
}

static void Stm32LedSetByLight(uint16_t als)
{
    const unsigned char *cmd = (als <= ALS_DARK_THRESHOLD) ?
        (const unsigned char *)"L1\n" : (const unsigned char *)"L0\n";

    MotorUartWrite(cmd, 3);
}

static void MotorSendSpeed(int leftSpeed, int rightSpeed)
{
    unsigned char frame[6];
    unsigned char leftDir = 0;
    unsigned char rightDir = 0;

    if (leftSpeed < 0) {
        leftDir = 1;
        leftSpeed = -leftSpeed;
    }
    if (rightSpeed < 0) {
        rightDir = 1;
        rightSpeed = -rightSpeed;
    }
    if (leftSpeed > 150) {
        leftSpeed = 150;
    }
    if (rightSpeed > 150) {
        rightSpeed = 150;
    }

    frame[0] = 0xFC;
    frame[1] = leftDir;
    frame[2] = (unsigned char)leftSpeed;
    frame[3] = rightDir;
    frame[4] = (unsigned char)rightSpeed;
    frame[5] = 0xFD;
    MotorUartWrite(frame, sizeof(frame));
}

static void CarForwardStart(void)
{
    MotorSendSpeed(START_SPEED, START_SPEED);
}

static void CarForward(void)
{
    MotorSendSpeed(CRUISE_SPEED, CRUISE_SPEED);
}

static void CarBackward(void)
{
    MotorSendSpeed(-BACKWARD_SPEED, -BACKWARD_SPEED);
}

static void CarTurnLeft(void)
{
    MotorSendSpeed(-TURN_SPEED, TURN_SPEED);
}

static void CarTurnRight(void)
{
    MotorSendSpeed(TURN_SPEED, -TURN_SPEED);
}

static void CarObstacleTurnRight(void)
{
    MotorSendSpeed(TURN_FAST_SPEED, TURN_SLOW_SPEED);
}

static void CarStop(void)
{
    MotorSendSpeed(0, 0);
}

static unsigned int MotorUartInit(void)
{
    WifiIotUartAttribute uartAttr = {
        .baudRate = MOTOR_BAUD_RATE,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    return UartInit(MOTOR_UART, &uartAttr, NULL);
}

static unsigned int BtUartInit(void)
{
    WifiIotUartAttribute uartAttr = {
        .baudRate = BT_BAUD_RATE,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_0, WIFI_IOT_IO_FUNC_GPIO_0_UART1_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_1, WIFI_IOT_IO_FUNC_GPIO_1_UART1_RXD);
    return UartInit(BT_UART, &uartAttr, NULL);
}

static void IrGpioInit(void)
{
    IoSetFunc(IR_LEFT_IO_NAME, IR_LEFT_IO_FUNC);
    IoSetFunc(IR_RIGHT_IO_NAME, IR_RIGHT_IO_FUNC);
    (void)hi_gpio_set_dir(HI_GPIO_IDX_14, HI_GPIO_DIR_IN);
    (void)hi_gpio_set_dir(HI_GPIO_IDX_13, HI_GPIO_DIR_IN);
}

static IrState ReadIrState(void)
{
    IrState state = {
        .left = HI_GPIO_VALUE0,
        .right = HI_GPIO_VALUE0,
        .leftRet = HI_ERR_SUCCESS,
        .rightRet = HI_ERR_SUCCESS,
    };

    state.leftRet = hi_gpio_get_input_val(HI_GPIO_IDX_14, &state.left);
    state.rightRet = hi_gpio_get_input_val(HI_GPIO_IDX_13, &state.right);
    return state;
}

static unsigned char IsReadOk(const IrState *state)
{
    return (state->leftRet == HI_ERR_SUCCESS && state->rightRet == HI_ERR_SUCCESS) ? 1 : 0;
}

static unsigned char IsTableSafe(const IrState *state)
{
    return (state->left != IR_EDGE_VALUE && state->right != IR_EDGE_VALUE) ? 1 : 0;
}

static unsigned char IsTableEdge(const IrState *state)
{
    return (state->left == IR_EDGE_VALUE || state->right == IR_EDGE_VALUE) ? 1 : 0;
}

static TurnDirection GetAvoidTurnDirection(const IrState *state)
{
    if (state->left == IR_EDGE_VALUE && state->right != IR_EDGE_VALUE) {
        return TURN_RIGHT;
    }
    if (state->right == IR_EDGE_VALUE && state->left != IR_EDGE_VALUE) {
        return TURN_LEFT;
    }
    return TURN_RIGHT;
}

static void ReportIrState(const char *prefix, const IrState *state)
{
    if (!IsReadOk(state)) {
        printf("%s read failed, leftRet=0x%x rightRet=0x%x\r\n", prefix, state->leftRet, state->rightRet);
    } else {
        printf("%s L=%u R=%u\r\n", prefix, state->left, state->right);
    }
}

static void ServoPulse(unsigned int dutyUs)
{
    GpioSetOutputVal(SG90_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(dutyUs);
    GpioSetOutputVal(SG90_GPIO, WIFI_IOT_GPIO_VALUE0);
    hi_udelay(SG90_PWM_PERIOD_US - dutyUs);
}

static void ServoHold(unsigned int dutyUs)
{
    for (int i = 0; i < SG90_HOLD_PULSES; i++) {
        ServoPulse(dutyUs);
    }
}

static void UltrasonicGpioInit(void)
{
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_2, WIFI_IOT_IO_FUNC_GPIO_2_GPIO);
    GpioSetDir(SG90_GPIO, WIFI_IOT_GPIO_DIR_OUT);

    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_7, WIFI_IOT_IO_FUNC_GPIO_7_GPIO);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_8, WIFI_IOT_IO_FUNC_GPIO_8_GPIO);
    GpioSetDir(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_DIR_OUT);
    GpioSetDir(HCSR04_ECHO_GPIO, WIFI_IOT_GPIO_DIR_IN);
    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    ServoHold(SG90_LEFT_DUTY_US);
    ServoHold(SG90_CENTER_DUTY_US);
    ServoHold(SG90_RIGHT_DUTY_US);
    ServoHold(SG90_CENTER_DUTY_US);
}

static unsigned int WaitEchoValue(WifiIotGpioValue expectValue, unsigned int timeoutUs)
{
    WifiIotGpioValue value = WIFI_IOT_GPIO_VALUE0;
    unsigned int start = hi_get_us();

    while ((unsigned int)(hi_get_us() - start) < timeoutUs) {
        GpioGetInputVal(HCSR04_ECHO_GPIO, &value);
        if (value == expectValue) {
            return hi_get_us();
        }
    }

    return 0;
}

static float Hcsr04GetDistance(void)
{
    unsigned int startTime;
    unsigned int endTime;
    unsigned int echoTime;

    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE1);
    hi_udelay(20);
    GpioSetOutputVal(HCSR04_TRIG_GPIO, WIFI_IOT_GPIO_VALUE0);

    startTime = WaitEchoValue(WIFI_IOT_GPIO_VALUE1, HCSR04_ECHO_TIMEOUT_US);
    if (startTime == 0) {
        return INVALID_DISTANCE_CM;
    }

    endTime = WaitEchoValue(WIFI_IOT_GPIO_VALUE0, HCSR04_ECHO_TIMEOUT_US);
    if (endTime == 0 || endTime <= startTime) {
        return INVALID_DISTANCE_CM;
    }

    echoTime = endTime - startTime;
    return (float)echoTime * 0.034f / 2.0f;
}

static void AvoidObstacle(void)
{
    unsigned int turnTime = 0;
    TurnDirection turnDirection = TURN_RIGHT;
    float leftDistance;
    float rightDistance;

    PrintTaskRun(1, "ultrasonic obstacle avoidance");
    CarStop();
    osDelay(100);

    ServoHold(SG90_LEFT_DUTY_US);
    osDelay(80);
    leftDistance = Hcsr04GetDistance();

    ServoHold(SG90_RIGHT_DUTY_US);
    osDelay(80);
    rightDistance = Hcsr04GetDistance();

    ServoHold(SG90_CENTER_DUTY_US);
    osDelay(80);

    if (leftDistance > rightDistance) {
        turnDirection = TURN_LEFT;
    }
    printf("Obstacle scan: left=%.1f cm right=%.1f cm, turn=%s\r\n",
        leftDistance, rightDistance, (turnDirection == TURN_LEFT) ? "left" : "right");

    CarBackward();
    osDelay(OBSTACLE_BACKWARD_TIME_MS);
    CarStop();
    osDelay(60);

    while (turnTime < MAX_OBSTACLE_TURN_TIME_MS) {
        float distance;
        IrState ir = ReadIrState();

        if (IsReadOk(&ir) && IsTableEdge(&ir)) {
            CarStop();
            printf("Obstacle turn stopped by table edge.\r\n");
            return;
        }

        if (turnDirection == TURN_LEFT) {
            CarTurnLeft();
        } else {
            CarObstacleTurnRight();
        }
        osDelay(OBSTACLE_TURN_TIME_MS);
        turnTime += OBSTACLE_TURN_TIME_MS;

        CarStop();
        osDelay(80);

        distance = Hcsr04GetDistance();
        printf("Obstacle turn check: %.1f cm\r\n", distance);
        if (distance > OBSTACLE_DISTANCE_CM) {
            printf("Obstacle clear, distance=%.1f cm\r\n", distance);
            break;
        }
    }

    CarStop();
    osDelay(80);
}

static void ApplyBtMode(void)
{
    switch (g_btMode) {
        case BT_MODE_STOP:
            CarStop();
            break;
        case BT_MODE_FORWARD:
            CarForward();
            break;
        case BT_MODE_BACKWARD:
            CarBackward();
            break;
        case BT_MODE_LEFT:
            CarTurnLeft();
            break;
        case BT_MODE_RIGHT:
            CarTurnRight();
            break;
        case BT_MODE_AUTO:
        default:
            CarForward();
            break;
    }
}

static void SafetyCarTask(void *arg)
{
    CarState state = CAR_STATE_FORWARD;
    TurnDirection turnDirection = TURN_RIGHT;
    unsigned int elapsedMs = 0;
    unsigned int forwardElapsedMs = 0;
    unsigned int reportElapsedMs = 0;
    IrState lastIr = {
        .left = HI_GPIO_VALUE0,
        .right = HI_GPIO_VALUE0,
        .leftRet = HI_ERR_SUCCESS,
        .rightRet = HI_ERR_SUCCESS,
    };

    (void)arg;

    PrintTaskRun(1, "car safety: IR edge protection + ultrasonic obstacle avoidance");
    CarForwardStart();

    while (1) {
        IrState ir = ReadIrState();
        float distance = INVALID_DISTANCE_CM;

        reportElapsedMs += SAMPLE_PERIOD_MS;

        if (!IsReadOk(&ir)) {
            CarStop();
            ReportIrState("IR", &ir);
            osDelay(SAMPLE_PERIOD_MS);
            continue;
        }

        if (ir.left != lastIr.left || ir.right != lastIr.right) {
            ReportIrState("IR changed", &ir);
        }
        lastIr = ir;

        if (IsTableEdge(&ir) && state == CAR_STATE_FORWARD) {
            PrintTaskRun(1, "table edge detected, brake/back/turn");
            CarStop();
            turnDirection = GetAvoidTurnDirection(&ir);
            state = CAR_STATE_BRAKE;
            elapsedMs = 0;
        }

        if (state == CAR_STATE_FORWARD) {
            distance = Hcsr04GetDistance();
        }

        if (reportElapsedMs >= 1000) {
            reportElapsedMs = 0;
            printf("Task 1 running: car safety, IR L=%u R=%u distance=%.1f cm mode=%u\r\n",
                ir.left, ir.right, distance, (unsigned int)g_btMode);
        }

        switch (state) {
            case CAR_STATE_FORWARD:
                if (distance > 0.0f && distance <= OBSTACLE_DISTANCE_CM) {
                    printf("Obstacle detected: %.1f cm\r\n", distance);
                    AvoidObstacle();
                    forwardElapsedMs = 0;
                } else if (g_btMode == BT_MODE_AUTO) {
                    forwardElapsedMs += SAMPLE_PERIOD_MS;
                    if (forwardElapsedMs < START_BOOST_TIME_MS) {
                        CarForwardStart();
                    } else {
                        CarForward();
                    }
                } else {
                    ApplyBtMode();
                }
                break;

            case CAR_STATE_BRAKE:
                elapsedMs += SAMPLE_PERIOD_MS;
                if (elapsedMs >= BRAKE_TIME_MS) {
                    PrintTaskRun(1, "back away from table edge");
                    CarBackward();
                    state = CAR_STATE_BACKWARD;
                    elapsedMs = 0;
                }
                break;

            case CAR_STATE_BACKWARD:
                elapsedMs += SAMPLE_PERIOD_MS;
                if (elapsedMs >= BACKWARD_TIME_MS) {
                    PrintTaskRun(1, "turn away from table edge");
                    if (turnDirection == TURN_LEFT) {
                        CarTurnLeft();
                    } else {
                        CarTurnRight();
                    }
                    state = CAR_STATE_EDGE_TURN;
                    elapsedMs = 0;
                }
                break;

            case CAR_STATE_EDGE_TURN:
                elapsedMs += SAMPLE_PERIOD_MS;
                if (elapsedMs >= EDGE_TURN_TIME_MS && IsTableSafe(&ir)) {
                    PrintTaskRun(1, "resume forward after edge avoidance");
                    CarForwardStart();
                    state = CAR_STATE_FORWARD;
                    elapsedMs = 0;
                    forwardElapsedMs = 0;
                }
                break;

            default:
                CarStop();
                state = CAR_STATE_FORWARD;
                elapsedMs = 0;
                forwardElapsedMs = 0;
                break;
        }

        osDelay(SAMPLE_PERIOD_MS);
    }
}

static void SensorDisplayTask(void *arg)
{
    uint16_t ir = 0;
    uint16_t als = 0;
    uint16_t ps = 0;
    float temperature = 0.0f;
    float humidity = 0.0f;
    char line0[24] = {0};
    char line1[24] = {0};
    char line2[24] = {0};
    char line3[24] = {0};

    (void)arg;

    PrintTaskRun(2, "OLED display: SHT20 temperature/humidity + AP3216C IR/ALS/PS");
    AP3216C_I2cBusInit();

    if (SSD1306_Init() != 0) {
        printf("SSD1306 init failed.\r\n");
        return;
    }
    if (SHT20_Init() != 0) {
        printf("SHT20 init failed.\r\n");
    }
    if (AP3216C_Init() != 0) {
        printf("AP3216C init failed.\r\n");
    }

    while (1) {
        int shtOk = (SHT20_ReadData(&temperature, &humidity) == 0);
        int apOk = (AP3216C_ReadData(&ir, &als, &ps) == 0);

        (void)memset(line0, 0, sizeof(line0));
        (void)memset(line1, 0, sizeof(line1));
        (void)memset(line2, 0, sizeof(line2));
        (void)memset(line3, 0, sizeof(line3));

        if (shtOk) {
            (void)snprintf(line0, sizeof(line0), "T:%.1fC H:%.1f%%", temperature, humidity);
        } else {
            (void)snprintf(line0, sizeof(line0), "SHT20 read fail");
        }

        if (apOk) {
            CarLedSetByLight(als);
            Stm32LedSetByLight(als);
            (void)snprintf(line1, sizeof(line1), "IR:%u", (unsigned int)ir);
            (void)snprintf(line2, sizeof(line2), "ALS:%u", (unsigned int)als);
            (void)snprintf(line3, sizeof(line3), "PS:%u", (unsigned int)ps);
            printf("Task 2 running: OLED update, temp=%.1fC humi=%.1f%% IR=%u ALS=%u PS=%u LED=%s\r\n",
                temperature, humidity, (unsigned int)ir, (unsigned int)als, (unsigned int)ps,
                (als <= ALS_DARK_THRESHOLD) ? "ON" : "OFF");
        } else {
            (void)snprintf(line1, sizeof(line1), "AP3216C fail");
            printf("Task 2 running: OLED update, AP3216C read failed\r\n");
        }

        SSD1306_CLS();
        SSD1306_ShowStr(0, 0, (uint8_t *)line0, OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 2, (uint8_t *)line1, OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 4, (uint8_t *)line2, OLED_TEXT_SIZE);
        SSD1306_ShowStr(0, 6, (uint8_t *)line3, OLED_TEXT_SIZE);

        osDelay(1000);
    }
}

static void SetBtModeFromBuffer(const unsigned char *buffer)
{
    if (strchr((const char *)buffer, 'A') != NULL || strchr((const char *)buffer, 'a') != NULL) {
        g_btMode = BT_MODE_AUTO;
    } else if (strchr((const char *)buffer, 'S') != NULL || strchr((const char *)buffer, 's') != NULL) {
        g_btMode = BT_MODE_STOP;
    } else if (strchr((const char *)buffer, 'F') != NULL || strchr((const char *)buffer, 'f') != NULL) {
        g_btMode = BT_MODE_FORWARD;
    } else if (strchr((const char *)buffer, 'B') != NULL || strchr((const char *)buffer, 'b') != NULL) {
        g_btMode = BT_MODE_BACKWARD;
    } else if (strchr((const char *)buffer, 'L') != NULL || strchr((const char *)buffer, 'l') != NULL) {
        g_btMode = BT_MODE_LEFT;
    } else if (strchr((const char *)buffer, 'R') != NULL || strchr((const char *)buffer, 'r') != NULL) {
        g_btMode = BT_MODE_RIGHT;
    }
}

static void BluetoothTask(void *arg)
{
    unsigned char buffer[BT_BUFFER_SIZE] = {0};
    unsigned int idleElapsedMs = 0;

    (void)arg;
    PrintTaskRun(3, "Bluetooth UART1 communication");
    (void)UartWrite(BT_UART, (const unsigned char *)"BT ready: A/S/F/B/L/R\r\n", 23);

    while (1) {
        int readLen = UartRead(BT_UART, buffer, BT_BUFFER_SIZE - 1);
        if (readLen > 0) {
            idleElapsedMs = 0;
            if (readLen >= BT_BUFFER_SIZE) {
                readLen = BT_BUFFER_SIZE - 1;
            }
            buffer[readLen] = '\0';
            printf("Task 3 running: Bluetooth recv: %s\r\n", buffer);
            SetBtModeFromBuffer(buffer);
            (void)UartWrite(BT_UART, (const unsigned char *)"echo: ", 6);
            (void)UartWrite(BT_UART, buffer, (unsigned int)readLen);
            (void)UartWrite(BT_UART, (const unsigned char *)"\r\n", 2);
        } else {
            idleElapsedMs += BT_READ_IDLE_MS;
            if (idleElapsedMs >= 5000) {
                idleElapsedMs = 0;
                PrintTaskRun(3, "Bluetooth UART1 waiting for command");
            }
        }

        osDelay(BT_READ_IDLE_MS);
    }
}

static void CreateThread(const char *name, osThreadFunc_t func, uint32_t stackSize)
{
    osThreadAttr_t attr;

    attr.name = name;
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = stackSize;
    attr.priority = TASK_PRIO;

    if (osThreadNew(func, NULL, &attr) == NULL) {
        printf("Failed to create %s task.\r\n", name);
    }
}

static void SumEntry(void)
{
    unsigned int ret;
    unsigned int btRet;

    WatchDogDisable();
    GpioInit();
    CarLedInit();
    IrGpioInit();
    UltrasonicGpioInit();

    ret = MotorUartInit();
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init motor UART2, err code: 0x%x\r\n", ret);
        return;
    }
    g_motorUartMutex = osMutexNew(NULL);
    if (g_motorUartMutex == NULL) {
        printf("Failed to create motor UART mutex.\r\n");
    }

    btRet = BtUartInit();
    if (btRet != WIFI_IOT_SUCCESS) {
        printf("Failed to init Bluetooth UART1, err code: 0x%x. UART1 may be occupied by debug serial.\r\n", btRet);
    }

    printf("10.0SUM project start.\r\n");
    CreateThread("Task1CarSafety", SafetyCarTask, CAR_TASK_STACK_SIZE);
    CreateThread("Task2SensorOLED", SensorDisplayTask, SENSOR_TASK_STACK_SIZE);
    CreateThread("Task3Bluetooth", BluetoothTask, BT_TASK_STACK_SIZE);
}

APP_FEATURE_INIT(SumEntry);
