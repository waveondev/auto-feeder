#ifndef __APP_HX711_H__
#define __APP_HX711_H__

#include "esp_log.h"
void HX711_cal_init(uint16_t cal);
bool HX711_init(void);
void HX711_Sensing(void);
float loadcell_data_get(void);
#endif

