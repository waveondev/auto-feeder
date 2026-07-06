#ifndef __APP_MOTER_H__
#define __APP_MOTER_H__
void init_motor_ledc(void);
// 모터 제어 함수 (speed: -1023 ~ 1023)
void set_motor_speed(int speed);
void set_motor_speed_percent(int percentage) ;

void start_motor_with_boost(int target_percentage, int duration_sec);
#endif

