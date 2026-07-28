
#ifndef __BLE_TRACKER_ID_H__
#define __BLE_TRACKER_ID_H__
#include "esp_log.h"
typedef struct {
    uint8_t addr[6];
    char name[32];
    int8_t rssi;
} dev_info_t;
typedef struct{
    char Device_ID[32];
    uint32_t total_Device_Time;
    uint32_t Device_Time;
    uint32_t Disable_Time; 
    uint32_t diff_Time;           
    uint32_t Enable;
    uint32_t Water_intake;
    dev_info_t dev_info;
}Tracker_Device_t;
Tracker_Device_t* Get_Tracker_Device(uint8_t* addr);
bool GetTracker_Id_active(void);
void Tracker_In_ID(dev_info_t* dev_info, char* Tracker_ID);
void Create_Tracker_Capture_Task(void);
void dump_tracker_all_devices(void);
void Tracker_waterintake_end(uint32_t Weight);
#endif
