#ifndef __MOTION_TASK_H__
#define __MOTION_TASK_H__
#include "esp_log.h"
#include "ble_parse.h"
#include "ble_task.h"

typedef enum
{
  HX711_Case_cal    = 0,
  HX711_Scale_cal   = 1,
  HX711_GetValue    = 2,
  IR_GetValue       = 3,  
  LED_SetValue      = 4,    
}Setting_cmd_e;

void Setting_callback(Motion_Packet_t* Motion_Packet);
void Setting_Enable(Setting_cmd_e cmd);
#endif

