#ifndef __AWS_IOT_TASK_H__
#define __AWS_IOT_TASK_H__
#include "tx_mqtt.h"
#include "ble_parse.h"
/**
 * @brief AWS IoT 및 Fleet Provisioning을 수행하는 백그라운드 태스크를 생성합니다.
 */
void aws_iot_task_init(void);
bool mqtt_queue_send(messege_tx_mqtt_cmd_e cmd);
void tracker_mqtt_queue_send(messege_tx_mqtt_cmd_e cmd, uint8_t* mac, Motion_Packet_t* packet,uint32_t data_len,  pack_data* data );
#endif /* __AWS_IOT_TASK_H__ */