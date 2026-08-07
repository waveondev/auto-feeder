#ifndef __APP_SLID_MOTER_H__
#define __APP_SLID_MOTER_H__
#include <stdbool.h>

void init_motor_ledc(void);
// 모터 제어 함수 (speed: -1023 ~ 1023)
void Sliding_CW(int speed);
void Sliding_CCW(int speed);
void Sliding_break(void);
void Sliding_coast(void);

typedef enum{
    MOTOR_CW        = 0,
    MOTOR_CCW       = 1,
    MOTOR_COAST,
    MOTOR_BREAK,      
}Slid_Moter_e;
typedef enum 
{
    SLID_OPEN = 0,
    SLID_OPENING,
    SLID_CLOSING,
    SLID_CLOSE,
} Slid_state_e;

Slid_state_e Slide_get_state(void);

#endif

