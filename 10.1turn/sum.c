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
#include "huawei_iot.h"
#include "ohos_init.h"
#include "wifi_connect.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"

#define MOTOR_UART WIFI_IOT_UART_IDX_2
#define MOTOR_BAUD_RATE 115200

#define CAR_LED_GPIO WIFI_IOT_IO_NAME_GPIO_6
#define CAR_LED_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_6_GPIO
#define ALS_DARK_THRESHOLD 50

#ifndef WIFI_SSID
#define WIFI_SSID "iPhone"
#endif

#ifndef WIFI_PSK
#define WIFI_PSK "nihao1009"
#endif

#ifndef HUAWEI_IOT_HOST
#define HUAWEI_IOT_HOST "3c95083845.st1.iotda-device.cn-north-4.myhuaweicloud.com"
#endif

#ifndef HUAWEI_IOT_PORT
#define HUAWEI_IOT_PORT 1883
#endif

#ifndef HUAWEI_DEVICE_ID
#define HUAWEI_DEVICE_ID "6a9643457f2e6c302f94fcf9_qstcar"
#endif

#ifndef HUAWEI_CLIENT_ID
#define HUAWEI_CLIENT_ID "6a9643457f2e6c302f94fcf9_qstcar_0_1_2026090103"
#endif

#ifndef HUAWEI_USERNAME
#define HUAWEI_USERNAME "6a9643457f2e6c302f94fcf9_qstcar"
#endif

#ifndef HUAWEI_PASSWORD
#define HUAWEI_PASSWORD "b1a98bed2b045f5f82d976d979d632c556def62af341d4bb6005090a5502041f"
#endif

#define IR_LEFT_IO_NAME WIFI_IOT_IO_NAME_GPIO_13
#define IR_RIGHT_IO_NAME WIFI_IOT_IO_NAME_GPIO_14
#define IR_LEFT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_13_GPIO
#define IR_RIGHT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_14_GPIO
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
#define CRUISE_SPEED 130
#define BACKWARD_SPEED 150
#define TURN_SPEED 130
#define TURN_FAST_SPEED 150
#define TURN_SLOW_SPEED (-50)

#define SAMPLE_PERIOD_MS 30
#define BRAKE_TIME_MS 150
#define BACKWARD_TIME_MS 80
#define EDGE_TURN_TIME_MS 180
#define START_BOOST_TIME_MS 50
#define TURN_CHECK_DELAY_MS 120
#define OBSTACLE_BACKWARD_TIME_MS 100
#define OBSTACLE_TURN_TIME_MS 300
#define MAX_OBSTACLE_TURN_TIME_MS 900

#define CAR_TASK_STACK_SIZE (1024 * 6)
#define SENSOR_TASK_STACK_SIZE (1024 * 6)
#define WIFI_TASK_STACK_SIZE (1024 * 6)
#define CLOUD_TASK_STACK_SIZE (1024 * 6)
#define TASK_PRIO 25
#define OLED_TEXT_SIZE 16
#define CLOUD_UPLOAD_PERIOD_MS 5000

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
    TURN_SIGNAL_OFF = 0,
    TURN_SIGNAL_LEFT,
    TURN_SIGNAL_RIGHT,
} TurnSignal;

typedef struct {
    hi_gpio_value left;
    hi_gpio_value right;
    unsigned int leftRet;
    unsigned int rightRet;
} IrState;

static TurnSignal g_turnSignal = TURN_SIGNAL_OFF;
static osMutexId_t g_motorUartMutex = NULL;
static osMutexId_t g_realtimeDataMutex = NULL;
static HuaweiCarData g_realtimeData = {0};
static volatile unsigned char g_wifiReady = 0;

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

static void UpdateRealtimeSensorData(float temperature, float humidity,
    uint16_t ir, uint16_t als, uint16_t ps)
{
    if (g_realtimeDataMutex != NULL) {
        (void)osMutexAcquire(g_realtimeDataMutex, osWaitForever);
    }

    g_realtimeData.temperature = temperature;
    g_realtimeData.humidity = humidity;
    g_realtimeData.apIr = (int)ir;
    g_realtimeData.apAls = (int)als;
    g_realtimeData.apPs = (int)ps;
    g_realtimeData.ledOn = (als <= ALS_DARK_THRESHOLD) ? 1 : 0;

    if (g_realtimeDataMutex != NULL) {
        (void)osMutexRelease(g_realtimeDataMutex);
    }
}

static void UpdateRealtimeSafetyData(const IrState *ir, float distance)
{
    if (ir == NULL) {
        return;
    }

    if (g_realtimeDataMutex != NULL) {
        (void)osMutexAcquire(g_realtimeDataMutex, osWaitForever);
    }

    g_realtimeData.edgeLeft = (ir->left == IR_EDGE_VALUE) ? 1 : 0;
    g_realtimeData.edgeRight = (ir->right == IR_EDGE_VALUE) ? 1 : 0;
    if (distance > 0.0f) {
        g_realtimeData.distanceCm = distance;
    }

    if (g_realtimeDataMutex != NULL) {
        (void)osMutexRelease(g_realtimeDataMutex);
    }
}

static void CopyRealtimeData(HuaweiCarData *data)
{
    if (data == NULL) {
        return;
    }

    if (g_realtimeDataMutex != NULL) {
        (void)osMutexAcquire(g_realtimeDataMutex, osWaitForever);
    }

    *data = g_realtimeData;

    if (g_realtimeDataMutex != NULL) {
        (void)osMutexRelease(g_realtimeDataMutex);
    }
}

static void Stm32LedSetByLight(uint16_t als)
{
    const unsigned char *cmd = (als <= ALS_DARK_THRESHOLD) ?
        (const unsigned char *)"L1\n" : (const unsigned char *)"L0\n";

    MotorUartWrite(cmd, 3);
}

static void Stm32TurnSignalSet(TurnSignal signal)
{
    const unsigned char *cmd = (const unsigned char *)"Y0\n";

    if (g_turnSignal == signal) {
        return;
    }

    if (signal == TURN_SIGNAL_LEFT) {
        cmd = (const unsigned char *)"YL\n";
    } else if (signal == TURN_SIGNAL_RIGHT) {
        cmd = (const unsigned char *)"YR\n";
    }

    g_turnSignal = signal;
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
    Stm32TurnSignalSet(TURN_SIGNAL_OFF);
    MotorSendSpeed(START_SPEED, START_SPEED);
}

static void CarForward(void)
{
    Stm32TurnSignalSet(TURN_SIGNAL_OFF);
    MotorSendSpeed(CRUISE_SPEED, CRUISE_SPEED);
}

static void CarBackward(void)
{
    Stm32TurnSignalSet(TURN_SIGNAL_OFF);
    MotorSendSpeed(-BACKWARD_SPEED, -BACKWARD_SPEED);
}

static void CarTurnLeft(void)
{
    Stm32TurnSignalSet(TURN_SIGNAL_LEFT);
    MotorSendSpeed(-TURN_SPEED, TURN_SPEED);
}

static void CarTurnRight(void)
{
    Stm32TurnSignalSet(TURN_SIGNAL_RIGHT);
    MotorSendSpeed(TURN_SPEED, -TURN_SPEED);
}

static void CarObstacleTurnRight(void)
{
    Stm32TurnSignalSet(TURN_SIGNAL_RIGHT);
    MotorSendSpeed(TURN_FAST_SPEED, TURN_SLOW_SPEED);
}

static void CarStop(void)
{
    Stm32TurnSignalSet(TURN_SIGNAL_OFF);
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

static void IrGpioInit(void)
{
    IoSetFunc(IR_LEFT_IO_NAME, IR_LEFT_IO_FUNC);
    IoSetFunc(IR_RIGHT_IO_NAME, IR_RIGHT_IO_FUNC);
    (void)hi_gpio_set_dir(HI_GPIO_IDX_13, HI_GPIO_DIR_IN);
    (void)hi_gpio_set_dir(HI_GPIO_IDX_14, HI_GPIO_DIR_IN);
}

static IrState ReadIrState(void)
{
    IrState state = {
        .left = HI_GPIO_VALUE0,
        .right = HI_GPIO_VALUE0,
        .leftRet = HI_ERR_SUCCESS,
        .rightRet = HI_ERR_SUCCESS,
    };

    state.leftRet = hi_gpio_get_input_val(HI_GPIO_IDX_13, &state.left);
    state.rightRet = hi_gpio_get_input_val(HI_GPIO_IDX_14, &state.right);
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
        UpdateRealtimeSafetyData(&ir, distance);

        if (reportElapsedMs >= 1000) {
            reportElapsedMs = 0;
            printf("Task 1 running: car safety, IR L=%u R=%u distance=%.1f cm\r\n",
                ir.left, ir.right, distance);
        }

        switch (state) {
            case CAR_STATE_FORWARD:
                if (distance > 0.0f && distance <= OBSTACLE_DISTANCE_CM) {
                    printf("Obstacle detected: %.1f cm\r\n", distance);
                    AvoidObstacle();
                    forwardElapsedMs = 0;
                } else {
                    forwardElapsedMs += SAMPLE_PERIOD_MS;
                    if (forwardElapsedMs < START_BOOST_TIME_MS) {
                        CarForwardStart();
                    } else {
                        CarForward();
                    }
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
            UpdateRealtimeSensorData(temperature, humidity, ir, als, ps);
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

static unsigned char WifiCredentialIsConfigured(void)
{
    if (strcmp(WIFI_SSID, "YOUR_WIFI_SSID") == 0 || WIFI_SSID[0] == '\0') {
        return 0;
    }
    if (strcmp(WIFI_PSK, "YOUR_WIFI_PASSWORD") == 0 || WIFI_PSK[0] == '\0') {
        return 0;
    }
    return 1;
}

static void WifiConnectTask(void *arg)
{
    int ret;

    (void)arg;

    PrintTaskRun(3, "WiFi connect");
    if (!WifiCredentialIsConfigured()) {
        printf("Task 3 running: WiFi credentials not configured, edit WIFI_SSID and WIFI_PSK in sum.c\r\n");
        return;
    }

    ret = WifiConnect(WIFI_SSID, WIFI_PSK);
    if (ret == 0) {
        g_wifiReady = 1;
        printf("Task 3 running: WiFi connect finished successfully\r\n");
    } else {
        printf("Task 3 running: WiFi connect failed, car tasks keep running\r\n");
    }
}

static unsigned char HuaweiIotCredentialIsConfigured(void)
{
    if (strcmp(HUAWEI_IOT_HOST, "YOUR_IOTDA_MQTT_HOST") == 0 || HUAWEI_IOT_HOST[0] == '\0') {
        return 0;
    }
    if (strcmp(HUAWEI_DEVICE_ID, "YOUR_DEVICE_ID") == 0 || HUAWEI_DEVICE_ID[0] == '\0') {
        return 0;
    }
    if (strcmp(HUAWEI_CLIENT_ID, "YOUR_CLIENT_ID") == 0 || HUAWEI_CLIENT_ID[0] == '\0') {
        return 0;
    }
    if (strcmp(HUAWEI_USERNAME, "YOUR_USERNAME") == 0 || HUAWEI_USERNAME[0] == '\0') {
        return 0;
    }
    if (strcmp(HUAWEI_PASSWORD, "YOUR_PASSWORD") == 0 || HUAWEI_PASSWORD[0] == '\0') {
        return 0;
    }
    return 1;
}

static void HuaweiCloudTask(void *arg)
{
    HuaweiIotConfig config = {
        .host = HUAWEI_IOT_HOST,
        .port = HUAWEI_IOT_PORT,
        .deviceId = HUAWEI_DEVICE_ID,
        .clientId = HUAWEI_CLIENT_ID,
        .username = HUAWEI_USERNAME,
        .password = HUAWEI_PASSWORD,
    };
    HuaweiCarData data;
    unsigned char connected = 0;

    (void)arg;

    PrintTaskRun(4, "Huawei Cloud MQTT realtime upload");
    if (!HuaweiIotCredentialIsConfigured()) {
        printf("Task 4 running: Huawei Cloud config not set, edit HUAWEI_* macros in sum.c\r\n");
        return;
    }

    while (!g_wifiReady) {
        printf("Task 4 running: wait WiFi ready before Huawei Cloud connect\r\n");
        osDelay(1000);
    }

    while (1) {
        if (!connected) {
            PrintTaskRun(4, "Huawei Cloud MQTT connect");
            connected = (HuaweiIotConnect(&config) == 0) ? 1 : 0;
            if (!connected) {
                osDelay(3000);
                continue;
            }
        }

        CopyRealtimeData(&data);
        if (HuaweiIotPublishCarData(&config, &data) != 0) {
            connected = 0;
            HuaweiIotClose();
            printf("Task 4 running: Huawei Cloud reconnect later\r\n");
        }

        osDelay(CLOUD_UPLOAD_PERIOD_MS);
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
    g_realtimeDataMutex = osMutexNew(NULL);
    if (g_realtimeDataMutex == NULL) {
        printf("Failed to create realtime data mutex.\r\n");
    }

    printf("10.0SUM project start.\r\n");
    CreateThread("Task1CarSafety", SafetyCarTask, CAR_TASK_STACK_SIZE);
    CreateThread("Task2SensorOLED", SensorDisplayTask, SENSOR_TASK_STACK_SIZE);
    CreateThread("Task3WiFi", WifiConnectTask, WIFI_TASK_STACK_SIZE);
    CreateThread("Task4HuaweiCloud", HuaweiCloudTask, CLOUD_TASK_STACK_SIZE);
}

APP_FEATURE_INIT(SumEntry);
