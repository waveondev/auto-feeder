#ifndef __BLE_TASK_H__
#define __BLE_TASK_H__

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ble_parse.h"
#define BLE_MSG_MAX_LEN    512

// 큐로 주고받을 데이터 구조체
typedef struct {
    uint16_t len;
    uint16_t conn_handle;
    uint8_t mac[6];
    uint8_t *data; 
} ble_data_msg_t;



typedef struct {
    /* [Group 1] 4바이트 정렬 멤버 (총 28 바이트) */
    TimerHandle_t xMotionTimer;     // 4B (Offset 0)
    TimerHandle_t xtimer;           // 4B (Offset 4)
    uint32_t timer_count;           // 4B (Offset 8)
    uint32_t first_delay_ms;        // 4B (Offset 12)
    uint32_t send_point;            // 4B (Offset 16)
    uint32_t total_point;           // 4B (Offset 20)
    uint32_t input_count;           // 4B (Offset 24)

    /* [Group 2] 20바이트 구조체 멤버 (총 40 바이트) */
    Motion_Packet_t start_res;     // 20B (Offset 28)
    Motion_Packet_t health_res;     // 20B (Offset 48)

    /* [Group 3] 소형 타입 조합으로 4바이트 묶기 (총 12 바이트) */
    uint8_t mac_addr[6];            // 6B (Offset 68)
    uint16_t conn_handle;           // 2B (Offset 74) -> 6 + 2 = 8B
    uint32_t start_res_seq;              // 4B (Offset 80)    
    bool is_connected;              // 1B (Offset 81)
    uint8_t* data;
} client_info_t;
/* Attributes State Machine */
void client_terminate(void);
uint16_t GetHandle(uint8_t index);
void print_connected_clients(void);
bool ble_send_data_to_queue(uint16_t* conn_handle, const uint8_t *data, uint16_t len);
void Motion_Timer_Set(const uint8_t *mac, bool state);
void ble_task_init(void);
void motion_msg_send(uint16_t conn_handle, uint8_t cmd,uint8_t sub_cmd);
void Tracker_All_Send(uint8_t cmd, uint8_t sub_cmd);
uint16_t get_conn_handle_by_mac(const uint8_t *mac);
client_info_t* get_conn_client_by_mac(const uint8_t *mac);
#endif
