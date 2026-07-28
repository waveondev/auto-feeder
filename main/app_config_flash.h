#ifndef __APP_CONFIG_FLASH_H__
#define __APP_CONFIG_FLASH_H__

#include "app_nvs.h"

#define BLE_DEVICENAME_LEN 32
#define WIFI_PASSWORD_LEN 64
typedef struct{
    uint32_t op_mode;
    uint32_t sliding_close_mode;
    uint32_t motor_current_limit;
    uint32_t sound_effect_idx;
    uint32_t food_low_limit;
    int32_t gate_way_rssi_th;
    float hx1_scale;                 // counts per gram
    int32_t hx1_offset;              // tare offset
    uint32_t case_raw_data;
    uint32_t tof_sense_threshold_l;
    uint32_t tof_sense_threshold_r;
    uint32_t motion_data_time;
    uint32_t EFFECTIVE_DWELL_TIME;
    uint32_t dispense_duration; // 토출시간 (ms)
    uint32_t dispense_amount_g;     // 토출량 (g)    
    char env_mode[16];               // "dev" 또는 "prod" 저장용
    char mqtt_url[128];              // (선택) 서버 주소 저장용
}app_config_t;



typedef struct{
    uint8_t conn_ssid[BLE_DEVICENAME_LEN];
    uint8_t conn_password[WIFI_PASSWORD_LEN];
}app_wifi_config_t;

typedef struct{
    uint8_t ble_device_name[BLE_DEVICENAME_LEN];
}app_ble_config_t;
void reset_all_nvs_data(void);
void app_nvs_save_set(void);
void wifi_nvs_save_set(void);
void ble_nvs_save_set(void);

app_config_t* get_app_config(void);
app_wifi_config_t* get_wifi_config(void);
app_ble_config_t* get_ble_config(void);

void load_app_configuration(void);

void load_wifi_configuration(void);

void load_ble_configuration(void);

void NVS_Flash_init(void);
void dump_all_configurations(void);
#endif

