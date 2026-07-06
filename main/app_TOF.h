#ifndef __APP_TOF_H__
#define __APP_TOF_H__

#include "esp_log.h"
bool VL53L0X_Detect(void);
bool TOF_VL53L0X_init(void);
void VL53L0X_Sensing(void);
#endif
