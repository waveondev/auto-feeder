#ifndef __TX_MQTT_H__
#define __TX_MQTT_H__

#include "esp_log.h"
#include "ble_parse.h"

typedef enum{
    MESSEGE_REGISTRATION        = 0x00,
    MESSEGE_BOOT,
    MESSEGE_ACCESS,
    MESSEGE_DIAGNOSTICS,
    MESSEGE_HEALTH,
    MESSEGE_DISPENSE,
    MESSEGE_INTAKE,


    AWS_MESSEGE_AWS_JOBS_GET = 0x80,



    TRACKER_MESSEGE_ACTIVITY    = 0xF0,
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

#define WEIGHT_ABNORMAL_INCREASE                (1<<0)
#define WEIGHT_ABNORMAL_DECREASE                (1<<1)
#define WEIGHT_SENSOR_ERR                       (1<<2)
#define MOTOR_SCREW_ERR                         (1<<3)
#define MOTOR_SCREW_JAMMED                      (1<<4)
#define MOTOR_SLIDING_ERR                       (1<<5)
#define MOTOR_SLIDING_BLOCKED                   (1<<6)
#define MOTOR_VACUUM_ERR                        (1<<7)
#define VACUUM_FAIL                             (1<<8)
#define FOOD_LOW                                (1<<9)
#define FOOD_EMPTY                              (1<<10)


void Send_cJSON_Messege(messege_tx_mqtt_cmd_e cmd);

void Send_cJSON_Messege_for_tracker(tracker_mqtt_packet_t* tracker_mqtt_packet);
void feeder_fault_enable(uint16_t status, bool count);
void feeder_fault_disable(uint16_t status,bool count);
#endif

