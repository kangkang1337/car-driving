#ifndef __SYSTEM_CONTROL_H
#define __SYSTEM_CONTROL_H

#include "sys.h"

extern int L_speed;
extern int R_speed;
extern int Motor_A;
extern int Motor_B;

int Incremental_PID_A(int encoder, int target);
int Incremental_PID_B(int encoder, int target);
int Rs_To_CPR(float rps);
void System_Control(void);

#endif
