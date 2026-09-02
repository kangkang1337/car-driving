#include "control_system.h"
#include "encoder.h"
#include "motor.h"

#define PID_CONTROL_PERIOD_MS 100
#define MOTOR_PPR             700
#define ENCODER_MULTIPLE      4
#define TARGET_LEFT_RPS       2.2f
#define TARGET_RIGHT_RPS      2.3f

int L_speed = 0;
int R_speed = 0;
int Motor_A = 0;
int Motor_B = 0;

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

int Incremental_PID_A(int encoder, int target)
{
    const float kp = 7.0f;
    const float ki = 0.016f;
    const float kd = 0.003f;
    static int pwm = 0;
    static float last_error = 0.0f;
    static float prev_error = 0.0f;
    float error = (float)(target - encoder);

    pwm += (int)(kp * (error - last_error) +
                 ki * error +
                 kd * (error - 2.0f * last_error + prev_error));
    pwm = limit_pwm(pwm);

    prev_error = last_error;
    last_error = error;

    return pwm;
}

int Incremental_PID_B(int encoder, int target)
{
    const float kp = 7.0f;
    const float ki = 0.016f;
    const float kd = 0.003f;
    static int pwm = 0;
    static float last_error = 0.0f;
    static float prev_error = 0.0f;
    float error = (float)(target - encoder);

    pwm += (int)(kp * (error - last_error) +
                 ki * error +
                 kd * (error - 2.0f * last_error + prev_error));
    pwm = limit_pwm(pwm);

    prev_error = last_error;
    last_error = error;

    return pwm;
}

int Rs_To_CPR(float rps)
{
    return (int)(rps * (float)(MOTOR_PPR * ENCODER_MULTIPLE) /
                 (1000.0f / PID_CONTROL_PERIOD_MS));
}

void System_Control(void)
{
    int target_a = Rs_To_CPR(TARGET_LEFT_RPS);
    int target_b = Rs_To_CPR(TARGET_RIGHT_RPS);

    L_speed = Read_Encoder(2);
    R_speed = Read_Encoder(3);

    Motor_A = Incremental_PID_A(L_speed, target_a);
    Motor_B = Incremental_PID_B(R_speed, target_b);

    Set_Pwm(Motor_A, Motor_B);

    printf("left  coder : %d\r\n", L_speed);
    printf("right coder : %d\r\n", R_speed);
    printf("TageA coder : %d\r\n", target_a);
    printf("TageB coder : %d\r\n", target_b);
    printf("Motor_A pwm : %d\r\n", Motor_A);
    printf("Motor_B pwm : %d\r\n", Motor_B);
}
