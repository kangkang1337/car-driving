#include "control_system.h"
#include "encoder.h"

int L_speed = 0;
int R_speed = 0;

void System_Control(void)
{
    L_speed = Read_Encoder(2);
    R_speed = Read_Encoder(3);

    printf("left  speed : %d\r\n", L_speed);
    printf("right speed : %d\r\n", R_speed);
}
