#ifndef __APP_SENSOR_H__
#define __APP_SENSOR_H__

#include "esp_log.h"
bool sensor_init(void);



bool Sliding_Back_Enable(void);
bool Sliding_Front_Enable(void);
bool Feed_Front_Enable(void);
bool Food_Door_Detected_State(void);
bool Food_Detected_State(void);
#endif

