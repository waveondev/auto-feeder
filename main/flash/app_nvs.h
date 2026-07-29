#ifndef __APP_NVS_H__
#define __APP_NVS_H__
#include "esp_log.h"
#include "esp_err.h"
#define APP_NAMESPACE           "WAVEON"
#define APP_KEY_CONFIGURATION   "CONFIG"
#define APP_KEY_WIFI_CONFIG     "WIFI"
#define APP_KEY_BLE_CONFIG      "BLE"
#define APP_KEY_MOTOR_TIME      "MOTOR_TIME"
#define APP_KEY_FILTER_TIME     "FILTER_TIME"
#define APP_REG_KEY             "is_prov_done"
void erase_app_configuration(void);
void erase_wifi_configuration(void);
void erase_ble_configuration(void);
void write_nvs_blob(const char* name, const char* key, const void* blob_data, size_t length);
esp_err_t read_nvs_blob(const char* name, const char* key, void* out_blob, size_t max_length);
esp_err_t read_nvs_uint(const char* name, const char* key, uint32_t* default_value);
void write_nvs_uint(const char* name, const char* key, uint32_t value);
int32_t read_nvs_int(const char* name, const char* key, int32_t default_value);
void write_nvs_int(const char* name, const char* key, int32_t value);
void write_nvs_memory(const char* name, const char* key, const char* data);
esp_err_t read_nvs_memory(const char* name, const char* key, char* out_data, uint16_t max_len);
void delete_nvs_namespace(const char* name);
void delete_nvs_key(const char* name, const char* key);
void print_all_nvs_keys(void);
bool read_nvs_registration_flag(void);
void write_nvs_registration_flag(bool done);
#endif
