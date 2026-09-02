#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "cmsis_os2.h"
#include "hi_errno.h"
#include "hi_gpio.h"
#include "hi_io.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"

#define IR_LEFT_IO_NAME WIFI_IOT_IO_NAME_GPIO_14
#define IR_RIGHT_IO_NAME WIFI_IOT_IO_NAME_GPIO_13
#define IR_LEFT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_14_GPIO
#define IR_RIGHT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_13_GPIO

#define MOTOR_UART WIFI_IOT_UART_IDX_2
#define MOTOR_BAUD_RATE 115200

#define TASK_STACK_SIZE (1024 * 4)
#define TASK_PRIO 25
#define SAMPLE_PERIOD_MS 50
#define BRAKE_TIME_MS 100
#define BACKWARD_TIME_MS 300
#define TURN_TIME_MS 450
#define START_SPEED 150
#define CRUISE_SPEED 120
#define START_BOOST_TIME_MS 300
#define BACKWARD_SPEED 150
#define TURN_SPEED 120

typedef enum {
    CAR_STATE_FORWARD = 0,
    CAR_STATE_BRAKE,
    CAR_STATE_BACKWARD,
    CAR_STATE_TURN,
} CarState;

typedef enum {
    TURN_RIGHT = 0,
    TURN_LEFT,
} TurnDirection;

typedef struct {
    hi_gpio_value left;
    hi_gpio_value right;
    unsigned int leftRet;
    unsigned int rightRet;
} IrState;

static void DebugSendText(const char *text)
{
    printf("%s", text);
}

static void DebugPrintf(const char *text)
{
    DebugSendText(text);
}

static unsigned int UartDeviceInit(void)
{
    WifiIotUartAttribute motorAttr = {
        .baudRate = MOTOR_BAUD_RATE,
        .dataBits = 8,
        .stopBits = 1,
        .parity = 0,
    };
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_11, WIFI_IOT_IO_FUNC_GPIO_11_UART2_TXD);
    IoSetFunc(WIFI_IOT_IO_NAME_GPIO_12, WIFI_IOT_IO_FUNC_GPIO_12_UART2_RXD);
    return UartInit(MOTOR_UART, &motorAttr, NULL);
}

static void IrGpioInit(void)
{
    GpioInit();
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
    return (state->left == HI_GPIO_VALUE0 && state->right == HI_GPIO_VALUE0) ? 1 : 0;
}

static unsigned char IsTableEdge(const IrState *state)
{
    return (state->left != HI_GPIO_VALUE0 || state->right != HI_GPIO_VALUE0) ? 1 : 0;
}

static void MotorControl(int leftSpeed, int rightSpeed)
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
    (void)UartWrite(MOTOR_UART, frame, sizeof(frame));
}

static void CarForward(void)
{
    MotorControl(CRUISE_SPEED, CRUISE_SPEED);
}

static void CarForwardStart(void)
{
    MotorControl(START_SPEED, START_SPEED);
}

static void CarBackward(void)
{
    MotorControl(-BACKWARD_SPEED, -BACKWARD_SPEED);
}

static void CarTurnLeft(void)
{
    MotorControl(-TURN_SPEED, TURN_SPEED);
}

static void CarTurnRight(void)
{
    MotorControl(TURN_SPEED, -TURN_SPEED);
}

static void CarStop(void)
{
    MotorControl(0, 0);
}

static TurnDirection GetAvoidTurnDirection(const IrState *state)
{
    if (state->left != HI_GPIO_VALUE0 && state->right == HI_GPIO_VALUE0) {
        return TURN_RIGHT;
    }
    if (state->right != HI_GPIO_VALUE0 && state->left == HI_GPIO_VALUE0) {
        return TURN_LEFT;
    }

    return TURN_RIGHT;
}

static void ReportIrState(const char *prefix, const IrState *state)
{
    char text[96];

    if (!IsReadOk(state)) {
        (void)snprintf(text, sizeof(text), "%s read failed, leftRet=0x%x rightRet=0x%x\r\n",
            prefix, state->leftRet, state->rightRet);
    } else {
        (void)snprintf(text, sizeof(text), "%s L=%u R=%u\r\n",
            prefix, state->left, state->right);
    }
    DebugPrintf(text);
}

static void TableEdgeBrakeTask(void *arg)
{
    CarState state = CAR_STATE_FORWARD;
    TurnDirection turnDirection = TURN_RIGHT;
    unsigned int elapsedMs = 0;
    unsigned int forwardElapsedMs = 0;
    unsigned char forwardReported = 0;
    unsigned char cruiseReported = 0;
    IrState lastIr = {
        .left = HI_GPIO_VALUE0,
        .right = HI_GPIO_VALUE0,
        .leftRet = HI_ERR_SUCCESS,
        .rightRet = HI_ERR_SUCCESS,
    };

    (void)arg;

    DebugPrintf("Table edge brake test start.\r\n");
    DebugPrintf("GPIO14=L GPIO13=R. L=0,R=0 forward; any side not 0 brake, back, turn, forward.\r\n");
    CarForwardStart();
    DebugPrintf("CAR forward start boost.\r\n");
    forwardReported = 1;

    while (1) {
        IrState ir = ReadIrState();

        if (!IsReadOk(&ir)) {
            CarStop();
            ReportIrState("IR", &ir);
            usleep(SAMPLE_PERIOD_MS * 1000);
            continue;
        }

        if (ir.left != lastIr.left || ir.right != lastIr.right) {
            ReportIrState("IR changed", &ir);
        }
        lastIr = ir;

        switch (state) {
            case CAR_STATE_FORWARD:
                if (IsTableEdge(&ir)) {
                    CarStop();
                    turnDirection = GetAvoidTurnDirection(&ir);
                    state = CAR_STATE_BRAKE;
                    elapsedMs = 0;
                    forwardElapsedMs = 0;
                    forwardReported = 0;
                    cruiseReported = 0;
                    DebugPrintf("EDGE detected. CAR brake.\r\n");
                } else if (IsTableSafe(&ir)) {
                    forwardElapsedMs += SAMPLE_PERIOD_MS;
                    if (forwardElapsedMs < START_BOOST_TIME_MS) {
                        CarForwardStart();
                    } else {
                        CarForward();
                        if (cruiseReported == 0) {
                            DebugPrintf("CAR forward cruise.\r\n");
                            cruiseReported = 1;
                        }
                    }
                    if (forwardReported == 0) {
                        DebugPrintf("CAR forward start boost.\r\n");
                        forwardReported = 1;
                    }
                } else {
                    CarStop();
                    forwardElapsedMs = 0;
                    forwardReported = 0;
                    cruiseReported = 0;
                }
                break;

            case CAR_STATE_BRAKE:
                elapsedMs += SAMPLE_PERIOD_MS;
                if (elapsedMs >= BRAKE_TIME_MS) {
                    CarBackward();
                    state = CAR_STATE_BACKWARD;
                    elapsedMs = 0;
                    DebugPrintf("CAR backward 300ms.\r\n");
                }
                break;

            case CAR_STATE_BACKWARD:
                elapsedMs += SAMPLE_PERIOD_MS;
                if (elapsedMs >= BACKWARD_TIME_MS) {
                    if (turnDirection == TURN_LEFT) {
                        CarTurnLeft();
                        DebugPrintf("CAR turn left.\r\n");
                    } else {
                        CarTurnRight();
                        DebugPrintf("CAR turn right.\r\n");
                    }
                    state = CAR_STATE_TURN;
                    elapsedMs = 0;
                }
                break;

            case CAR_STATE_TURN:
                elapsedMs += SAMPLE_PERIOD_MS;
                if (elapsedMs >= TURN_TIME_MS) {
                    CarForwardStart();
                    state = CAR_STATE_FORWARD;
                    elapsedMs = 0;
                    forwardElapsedMs = 0;
                    forwardReported = 1;
                    cruiseReported = 0;
                    DebugPrintf("CAR forward start boost.\r\n");
                }
                break;

            default:
                CarStop();
                state = CAR_STATE_FORWARD;
                elapsedMs = 0;
                forwardElapsedMs = 0;
                forwardReported = 0;
                cruiseReported = 0;
                break;
        }

        usleep(SAMPLE_PERIOD_MS * 1000);
    }
}

static void TableEdgeBrakeEntry(void)
{
    osThreadAttr_t attr;
    unsigned int ret;

    IrGpioInit();
    ret = UartDeviceInit();
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init motor UART2, err code: 0x%x\r\n", ret);
        return;
    }

    attr.name = "TableEdgeBrake";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = TASK_STACK_SIZE;
    attr.priority = TASK_PRIO;

    if (osThreadNew(TableEdgeBrakeTask, NULL, &attr) == NULL) {
        printf("Failed to create TableEdgeBrake task.\r\n");
    }
}

APP_FEATURE_INIT(TableEdgeBrakeEntry);
