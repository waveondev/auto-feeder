#ifndef __TX_MQTT_H__
#define __TX_MQTT_H__

#include "esp_log.h"
#include "ble_parse.h"

typedef enum{
    MESSEGE_REGISTRATION        = 0x00,
    MESSEGE_BOOT,
    MESSEGE_ACCESS,
    MESSEGE_DRINK,
    MESSEGE_DIAGNOSTICS,
    MESSEGE_HEALTH,

    TRACKER_MESSEGE_ACTIVITY    = 0x80,
    TRACKER_MESSEGE_DIAGNOSTICS,
    TRACKER_MESSEGE_HEALTH,
}messege_tx_mqtt_cmd_e;

typedef struct
{
    messege_tx_mqtt_cmd_e cmd;     
    uint8_t mac[6];    
    Motion_Packet_t packet;
    pack_data* data;
    uint32_t data_len;
}tracker_mqtt_packet_t;

#define WATER_LOW_FAULT                 (1<<0)
#define WATER_EMPTY_FAULT               (1<<1)
#define WATER_BOWL_DETACHED_FAULT       (1<<2)
#define WATER_LOADCELL_ERR              (1<<3)
#define WATER_SPLASHING_FAULT           (1<<4)
#define WATER_PUMP_ERR                  (1<<5)
#define WATER_TOF_SENSOR_ERR            (1<<6)
#define WATER_BLE_SCAN_ERR              (1<<7)
#define WATER_UV_FAIL                   (1<<8)
#define WATER_FILTER_WATER_EX           (1<<9)
#define WATER_FILTER_DEBRIS_EX          (1<<10)
#define WATER_MODECHANGE                (1<<11)


void Send_cJSON_Messege(messege_tx_mqtt_cmd_e cmd);

void Send_cJSON_Messege_for_tracker(tracker_mqtt_packet_t* tracker_mqtt_packet);
void water_fault_enable(uint16_t status);
void water_fault_disable(uint16_t status);
#endif

