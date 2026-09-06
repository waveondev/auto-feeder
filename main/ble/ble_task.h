#ifndef __BLE_TASK_H__
#define __BLE_TASK_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BLE_MSG_MAX_LEN    512

// 큐로 주고받을 데이터 구조체
typedef struct {
    uint16_t len;
    uint16_t conn_handle;
    uint8_t mac[6];
    uint8_t *data; 
} ble_data_msg_t;
/* Attributes State Machine */
void print_connected_clients(void);
bool ble_send_data_to_queue(uint16_t* conn_handle, const uint8_t *data, uint16_t len);
void Motion_Timer_Set(const uint8_t *mac, bool state);
void ble_task_init(void);
void motion_msg_send(uint16_t conn_handle, uint8_t cmd,uint8_t sub_cmd);
void Tracker_All_Send(uint8_t cmd, uint8_t sub_cmd);
uint16_t get_conn_handle_by_mac(const uint8_t *mac);
#endif
