
#ifndef __BLE_TRACKER_ID_H__
#define __BLE_TRACKER_ID_H__
#include "esp_log.h"

void Tracker_In_ID(char* Tracker_ID);
void Create_Tracker_Capture_Task(void);
void dump_tracker_all_devices(void);
void Tracker_waterintake_end(uint32_t Weight);
#endif
