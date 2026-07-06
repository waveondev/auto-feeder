#ifndef __APP_CONFIG_FLASH_H__
#define __APP_CONFIG_FLASH_H__

#include "app_nvs.h"

#define BLE_DEVICENAME_LEN 32
#define WIFI_PASSWORD_LEN 64
typedef struct{
    uint32_t op_mode;
    uint32_t pump_clean_duration;
    uint32_t filter_life_days;
    uint32_t min_weight_threshold;
    uint32_t splash_delta_g;
    int32_t gate_way_rssi_th;
    float hx1_scale;                 // counts per gram
    int32_t hx1_offset;              // tare offset
    uint32_t case_raw_data;
    uint32_t tof_sense_threshold_l;
    uint32_t tof_sense_threshold_r;
    uint32_t motion_data_time;
}app_config_t;



typedef struct{
    uint8_t conn_ssid[BLE_DEVICENAME_LEN];
    uint8_t conn_password[WIFI_PASSWORD_LEN];
}wifi_config_t;

typedef struct{
    uint8_t ble_device_name[BLE_DEVICENAME_LEN];
}ble_config_t;
void reset_all_nvs_data(void);
void app_nvs_save_set(void);
void wifi_nvs_save_set(void);
void ble_nvs_save_set(void);
app_config_t* get_app_config(void);
wifi_config_t* get_wifi_config(void);
ble_config_t* get_ble_config(void);
void load_app_configuration(void);

void load_wifi_configuration(void);

void load_ble_configuration(void);

void NVS_Flash_init(void);
void dump_all_configurations(void);
#endif

