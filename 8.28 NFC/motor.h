#ifndef __MOTOR_H
#define __MOTOR_H

#include "sys.h"

#define PWM_MAX 7199

void PWM_Init(u16 arr, u16 psc);
void Set_Pwm(int left_motor, int right_motor);
void Motor_Stop(void);

#endif
