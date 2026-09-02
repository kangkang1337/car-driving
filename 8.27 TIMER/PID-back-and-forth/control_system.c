#include "control_system.h"
#include "encoder.h"
#include "motor.h"

#define PID_CONTROL_PERIOD_MS 100
#define MOTOR_PPR             700
#define ENCODER_MULTIPLE      4
#define TARGET_LEFT_RPS       2.2f
#define TARGET_RIGHT_RPS      2.3f
#define BACKWARD_LEFT_RPS     2.7f
#define BACKWARD_RIGHT_RPS    2.7f
#define BACKWARD_MIN_PWM      1600
#define DRIVE_DISTANCE_COUNTS 14000
#define DRIVE_PAUSE_TICKS     5

int L_speed = 0;
int R_speed = 0;
int Motor_A = 0;
int Motor_B = 0;

typedef enum {
    DRIVE_FORWARD = 0,
    DRIVE_PAUSE,
    DRIVE_BACKWARD,
    DRIVE_STOP
} Drive_State;

static Drive_State drive_state = DRIVE_FORWARD;
static int distance_count = 0;
static int pause_count = 0;
static int pid_a_pwm = 0;
static int pid_b_pwm = 0;
static float pid_a_last_error = 0.0f;
static float pid_a_prev_error = 0.0f;
static float pid_b_last_error = 0.0f;
static float pid_b_prev_error = 0.0f;

static int limit_pwm(int pwm)
{
    if (pwm > PWM_MAX) {
        return PWM_MAX;
    }

    if (pwm < -PWM_MAX) {
        return -PWM_MAX;
    }

    return pwm;
}

static int abs_int(int value)
{
    return (value < 0) ? -value : value;
}

static void Reset_PID(void)
{
    pid_a_pwm = 0;
    pid_b_pwm = 0;
    pid_a_last_error = 0.0f;
    pid_a_prev_error = 0.0f;
    pid_b_last_error = 0.0f;
    pid_b_prev_error = 0.0f;
}

static int Apply_Min_Pwm(int pwm, int target, int encoder, int min_pwm)
{
    if (target > 0 && encoder < target && pwm < min_pwm) {
        return min_pwm;
    }

    if (target < 0 && encoder > target && pwm > -min_pwm) {
        return -min_pwm;
    }

    return pwm;
}

int Incremental_PID_A(int encoder, int target)
{
    const float kp = 7.0f;
    const float ki = 0.016f;
    const float kd = 0.003f;
    float error = (float)(target - encoder);

    pid_a_pwm += (int)(kp * (error - pid_a_last_error) +
                 ki * error +
                 kd * (error - 2.0f * pid_a_last_error + pid_a_prev_error));
    pid_a_pwm = limit_pwm(pid_a_pwm);

    pid_a_prev_error = pid_a_last_error;
    pid_a_last_error = error;

    return pid_a_pwm;
}

int Incremental_PID_B(int encoder, int target)
{
    const float kp = 7.0f;
    const float ki = 0.016f;
    const float kd = 0.003f;
    float error = (float)(target - encoder);

    pid_b_pwm += (int)(kp * (error - pid_b_last_error) +
                 ki * error +
                 kd * (error - 2.0f * pid_b_last_error + pid_b_prev_error));
    pid_b_pwm = limit_pwm(pid_b_pwm);

    pid_b_prev_error = pid_b_last_error;
    pid_b_last_error = error;

    return pid_b_pwm;
}

int Rs_To_CPR(float rps)
{
    return (int)(rps * (float)(MOTOR_PPR * ENCODER_MULTIPLE) /
                 (1000.0f / PID_CONTROL_PERIOD_MS));
}

void System_Control(void)
{
    int target_a = 0;
    int target_b = 0;
    int delta_count = 0;

    L_speed = Read_Encoder(2);
    R_speed = Read_Encoder(3);
    delta_count = (abs_int(L_speed) + abs_int(R_speed)) / 2;

    if (drive_state == DRIVE_FORWARD || drive_state == DRIVE_BACKWARD) {
        distance_count += delta_count;
    }

    if (drive_state == DRIVE_FORWARD) {
        if (distance_count >= DRIVE_DISTANCE_COUNTS) {
            drive_state = DRIVE_PAUSE;
            distance_count = 0;
            pause_count = 0;
            Reset_PID();
        }
    } else if (drive_state == DRIVE_PAUSE) {
        pause_count++;
        if (pause_count >= DRIVE_PAUSE_TICKS) {
            drive_state = DRIVE_BACKWARD;
            pause_count = 0;
            Reset_PID();
        }
    } else if (drive_state == DRIVE_BACKWARD) {
        if (distance_count >= DRIVE_DISTANCE_COUNTS) {
            drive_state = DRIVE_STOP;
            distance_count = 0;
            pause_count = 0;
            Reset_PID();
        }
    }

    if (drive_state == DRIVE_FORWARD) {
        target_a = Rs_To_CPR(TARGET_LEFT_RPS);
        target_b = Rs_To_CPR(TARGET_RIGHT_RPS);
    } else if (drive_state == DRIVE_BACKWARD) {
        target_a = Rs_To_CPR(-BACKWARD_LEFT_RPS);
        target_b = Rs_To_CPR(-BACKWARD_RIGHT_RPS);
    } else {
        Motor_A = 0;
        Motor_B = 0;
        Motor_Stop();
    }

    if (drive_state == DRIVE_FORWARD || drive_state == DRIVE_BACKWARD) {
        Motor_A = Incremental_PID_A(L_speed, target_a);
        Motor_B = Incremental_PID_B(R_speed, target_b);

        if (drive_state == DRIVE_BACKWARD) {
            Motor_A = Apply_Min_Pwm(Motor_A, target_a, L_speed, BACKWARD_MIN_PWM);
            Motor_B = Apply_Min_Pwm(Motor_B, target_b, R_speed, BACKWARD_MIN_PWM);
        }

        Set_Pwm(Motor_A, Motor_B);
    }

    printf("left  coder : %d\r\n", L_speed);
    printf("right coder : %d\r\n", R_speed);
    printf("TageA coder : %d\r\n", target_a);
    printf("TageB coder : %d\r\n", target_b);
    printf("Motor_A pwm : %d\r\n", Motor_A);
    printf("Motor_B pwm : %d\r\n", Motor_B);
    printf("State       : %d\r\n", drive_state);
    printf("Distance    : %d\r\n", distance_count);
}
