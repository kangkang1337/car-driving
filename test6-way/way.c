#include <stdio.h>
#include <stdint.h>

#include "cmsis_os2.h"
#include "hi_errno.h"
#include "hi_gpio.h"
#include "ohos_init.h"
#include "wifiiot_errno.h"
#include "wifiiot_gpio.h"
#include "wifiiot_gpio_ex.h"
#include "wifiiot_uart.h"
#include "wifiiot_watchdog.h"

#define MOTOR_UART WIFI_IOT_UART_IDX_2
#define MOTOR_BAUD_RATE 115200

#define IR_LEFT_IO_NAME WIFI_IOT_IO_NAME_GPIO_13
#define IR_RIGHT_IO_NAME WIFI_IOT_IO_NAME_GPIO_14
#define IR_LEFT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_13_GPIO
#define IR_RIGHT_IO_FUNC WIFI_IOT_IO_FUNC_GPIO_14_GPIO
#define IR_BLACK_VALUE HI_GPIO_VALUE1
#define IR_FLOOR_VALUE HI_GPIO_VALUE0

#define CRUISE_SPEED 125
#define CORRECT_FAST_SPEED 145
#define CORRECT_SLOW_SPEED 55

#define JUNCTION_LEFT_FAST_SPEED 145
#define JUNCTION_LEFT_SLOW_SPEED 55
#define JUNCTION_RIGHT_FAST_SPEED 145
#define JUNCTION_RIGHT_SLOW_SPEED 55
#define JUNCTION_BIAS_MS 260
#define JUNCTION_RELEASE_TIMEOUT_MS 700

#define SAMPLE_PERIOD_MS 10
#define START_BOOST_MS 250
#define START_BOOST_SPEED 135
#define MARKER_CONFIRM_MS 50
#define FINISH_DETECT_SPEED 80
#define FINISH_FIRST_RELEASE_TIMEOUT_MS 350
#define FINISH_WHITE_GAP_MAX_MS 350
#define FINISH_SECOND_CONFIRM_MS 10
#define END_BLINK_PERIOD_MS 300
#define OS_TICK_MS 10

#define WAY_TASK_STACK_SIZE (1024 * 4)
#define TASK_PRIO 25

typedef struct {
    hi_gpio_value left;
    hi_gpio_value right;
    unsigned int leftRet;
    unsigned int rightRet;
} IrState;

static osMutexId_t g_motorUartMutex = NULL;
static uint32_t g_markerCount = 0;

static void DelayMs(uint32_t ms)
{
    uint32_t ticks = (ms + OS_TICK_MS - 1) / OS_TICK_MS;
    osDelay(ticks == 0 ? 1 : ticks);
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

static void Stm32EndBlink(void)
{
    const unsigned char cmd[] = "E\n";
    MotorUartWrite(cmd, sizeof(cmd) - 1);
}

static void Stm32LedOff(void)
{
    const unsigned char cmd[] = "L0\n";
    MotorUartWrite(cmd, sizeof(cmd) - 1);
}

static void CarStop(void)
{
    MotorSendSpeed(0, 0);
}

static void CarForward(int speed)
{
    MotorSendSpeed(speed, speed);
}

static void CarCorrectLeft(void)
{
    MotorSendSpeed(CORRECT_SLOW_SPEED, CORRECT_FAST_SPEED);
}

static void CarCorrectRight(void)
{
    MotorSendSpeed(CORRECT_FAST_SPEED, CORRECT_SLOW_SPEED);
}

static void CarBiasLeft(void)
{
    MotorSendSpeed(JUNCTION_LEFT_SLOW_SPEED, JUNCTION_LEFT_FAST_SPEED);
}

static void CarBiasRight(void)
{
    MotorSendSpeed(JUNCTION_RIGHT_FAST_SPEED, JUNCTION_RIGHT_SLOW_SPEED);
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
        .left = IR_FLOOR_VALUE,
        .right = IR_FLOOR_VALUE,
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

static unsigned char IsBothBlack(const IrState *state)
{
    return (state->left == IR_BLACK_VALUE && state->right == IR_BLACK_VALUE) ? 1 : 0;
}

static unsigned char ConfirmBothBlack(void)
{
    uint32_t elapsed = 0;

    while (elapsed < MARKER_CONFIRM_MS) {
        IrState ir = ReadIrState();
        if (!IsReadOk(&ir) || !IsBothBlack(&ir)) {
            return 0;
        }
        DelayMs(SAMPLE_PERIOD_MS);
        elapsed += SAMPLE_PERIOD_MS;
    }

    return 1;
}

static unsigned char ConfirmBothBlackFor(uint32_t confirmMs)
{
    uint32_t elapsed = 0;

    while (elapsed < confirmMs) {
        IrState ir = ReadIrState();
        if (!IsReadOk(&ir) || !IsBothBlack(&ir)) {
            return 0;
        }
        DelayMs(SAMPLE_PERIOD_MS);
        elapsed += SAMPLE_PERIOD_MS;
    }

    return 1;
}

static void FollowLineStep(uint32_t *boostElapsed)
{
    IrState ir = ReadIrState();

    if (!IsReadOk(&ir)) {
        CarStop();
    } else if (ir.left == IR_BLACK_VALUE && ir.right == IR_FLOOR_VALUE) {
        CarCorrectLeft();
    } else if (ir.left == IR_FLOOR_VALUE && ir.right == IR_BLACK_VALUE) {
        CarCorrectRight();
    } else {
        if (*boostElapsed < START_BOOST_MS) {
            CarForward(START_BOOST_SPEED);
            *boostElapsed += SAMPLE_PERIOD_MS;
        } else {
            CarForward(CRUISE_SPEED);
        }
    }
}

static unsigned char IsFinishDoubleMarker(void)
{
    uint32_t whiteElapsed = 0;
    unsigned char firstReleased = 0;

    while (whiteElapsed < FINISH_FIRST_RELEASE_TIMEOUT_MS) {
        IrState ir = ReadIrState();

        if (IsReadOk(&ir) && !IsBothBlack(&ir)) {
            firstReleased = 1;
            break;
        }

        CarForward(FINISH_DETECT_SPEED);
        DelayMs(SAMPLE_PERIOD_MS);
        whiteElapsed += SAMPLE_PERIOD_MS;
    }

    if (!firstReleased) {
        printf("Marker release timeout while checking finish.\r\n");
        return 0;
    }

    whiteElapsed = 0;
    while (whiteElapsed < FINISH_WHITE_GAP_MAX_MS) {
        IrState ir = ReadIrState();

        if (IsReadOk(&ir) && IsBothBlack(&ir)) {
            if (ConfirmBothBlackFor(FINISH_SECOND_CONFIRM_MS)) {
                printf("Finish pattern detected: black-white-black, gap=%u ms.\r\n", whiteElapsed);
                return 1;
            }
        }

        CarForward(FINISH_DETECT_SPEED);
        DelayMs(SAMPLE_PERIOD_MS);
        whiteElapsed += SAMPLE_PERIOD_MS;
    }

    printf("Single marker: no second marker in %u ms.\r\n", FINISH_WHITE_GAP_MAX_MS);
    return 0;
}

static void ReleaseJunction(void)
{
    uint32_t elapsed = 0;

    while (elapsed < JUNCTION_RELEASE_TIMEOUT_MS) {
        IrState ir = ReadIrState();

        if (IsReadOk(&ir) && !IsBothBlack(&ir)) {
            return;
        }

        CarForward(FINISH_DETECT_SPEED);
        DelayMs(SAMPLE_PERIOD_MS);
        elapsed += SAMPLE_PERIOD_MS;
    }
}

static void HandleJunctionByCount(void)
{
    uint32_t elapsed = 0;

    g_markerCount++;
    printf("Junction marker count=%u.\r\n", g_markerCount);
    ReleaseJunction();

    while (elapsed < JUNCTION_BIAS_MS) {
        if ((g_markerCount % 2) == 1) {
            CarBiasLeft();
        } else {
            CarBiasRight();
        }
        DelayMs(SAMPLE_PERIOD_MS);
        elapsed += SAMPLE_PERIOD_MS;
    }
}

static void FinishAndBlink(void)
{
    printf("Double marker: finish reached.\r\n");
    CarStop();

    while (1) {
        Stm32EndBlink();
        DelayMs(END_BLINK_PERIOD_MS);
    }
}

static void HandleMarker(void)
{
    printf("Marker detected.\r\n");
    CarStop();
    DelayMs(80);

    if (IsFinishDoubleMarker()) {
        FinishAndBlink();
    }

    HandleJunctionByCount();
}

static void WayTask(void *arg)
{
    uint32_t boostElapsed = 0;
    IrState lastIr = {
        .left = IR_FLOOR_VALUE,
        .right = IR_FLOOR_VALUE,
        .leftRet = HI_ERR_SUCCESS,
        .rightRet = HI_ERR_SUCCESS,
    };

    (void)arg;

    printf("test6 way line-follow start. floor=0 black=1\r\n");
    Stm32LedOff();
    CarStop();
    DelayMs(300);

    while (1) {
        IrState ir = ReadIrState();

        if (!IsReadOk(&ir)) {
            printf("IR read failed, leftRet=0x%x rightRet=0x%x\r\n", ir.leftRet, ir.rightRet);
            CarStop();
            DelayMs(100);
            continue;
        }

        if (ir.left != lastIr.left || ir.right != lastIr.right) {
            printf("IR changed: L=%u R=%u\r\n", ir.left, ir.right);
            lastIr = ir;
        }

        if (IsBothBlack(&ir) && ConfirmBothBlack()) {
            HandleMarker();
            boostElapsed = 0;
        } else {
            FollowLineStep(&boostElapsed);
        }

        DelayMs(SAMPLE_PERIOD_MS);
    }
}

static void WayEntry(void)
{
    unsigned int ret;
    osThreadAttr_t attr;

    WatchDogDisable();
    GpioInit();
    IrGpioInit();

    ret = MotorUartInit();
    if (ret != WIFI_IOT_SUCCESS) {
        printf("Failed to init motor UART2, err code: 0x%x\r\n", ret);
        return;
    }

    g_motorUartMutex = osMutexNew(NULL);
    if (g_motorUartMutex == NULL) {
        printf("Failed to create motor UART mutex.\r\n");
    }

    attr.name = "WayTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = WAY_TASK_STACK_SIZE;
    attr.priority = TASK_PRIO;

    if (osThreadNew(WayTask, NULL, &attr) == NULL) {
        printf("Failed to create way task.\r\n");
    }
}

APP_FEATURE_INIT(WayEntry);
