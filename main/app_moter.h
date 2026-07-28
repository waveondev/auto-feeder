#ifndef __APP_MOTER_H__
#define __APP_MOTER_H__
#include <stdbool.h>
void motor_all_off(void);
void init_motor_ledc(void);
// 모터 제어 함수 (speed: -1023 ~ 1023)
void Sliding_CW(void);
void Sliding_CCW(void);
void Sliding_OFF(void);
void Feeder_CW(void);
void Feeder_CCW(void);
void Feeder_OFF(void);
void Accum_Set(bool status);
void motor_all_off(void);

void start_motor_with_boost(int target_percentage, bool Motor_CW_CCW);
#endif

