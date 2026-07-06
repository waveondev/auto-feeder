#ifndef __WIFI_TASK_H__
#define __WIFI_TASK_H__
void Wifi_Disconnect(void);
void wifi_init(void); 
uint16_t wifi_scan_start(void);
void Wifi_Connect(const char* target_ssid, const char* target_password);
#endif

