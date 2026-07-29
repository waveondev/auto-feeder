#ifndef __APP_SENSOR_H__
#define __APP_SENSOR_H__

#include "esp_log.h"
bool sensor_init(void);



bool Sliding_Back_Enable(void);
bool Sliding_Front_Enable(void);
bool IROUT1_Detected_State(void);
bool IROUT0_Detected_State(void);
#endif

