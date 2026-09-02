#ifndef __MOTOR_H
#define __MOTOR_H

#include "sys.h"

#define PWM_MAX 3599

void PWM_Init(u16 arr, u16 psc);
void Set_Pwm(int left_motor, int right_motor);
void Motor_Stop(void);
void stm32motor_control(int left_motor, int right_motor);
void car_stop(void);
void car_forward(void);
void car_left(void);
void car_right(void);
void car_backward(void);

#endif
