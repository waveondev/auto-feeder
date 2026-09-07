#ifndef __OPMODE_TASK_H__
#define __OPMODE_TASK_H__

#include "esp_log.h"
typedef enum {
    FEED_MODE_SCHEDULED_PORTION = 0, // 정량/정시 배식
    FEED_MODE_MANUAL_PORTION    = 1, // 정량 배식 (수동/즉시)
    FEED_MODE_FREE_FEEDING      = 2, // 자율 배식
    OP_MODE_TEST                = 3
} op_mode_e;

typedef enum {
    FEED_MODE_NONE = 0,
    FEED_MODE_SCHEDULED,
    FEED_MODE_MENUAL,
    FEED_MODE_DEVICE_BUTTON,
    FEED_MODE_AUTO_REFILL
} feed_mode_e;
typedef struct {
    float start_weight;
    float end_weight;
    float weight_delta;
    uint32_t duration_sec;
}INTAKE_Packet_t;

typedef struct {
    feed_mode_e trigger;
    float target_amount;
    float dispensed_amount;
    float residual_weight;
    bool status;
}DISPENSE_Packet_t;

void Night_Mode(bool state);
void Clean_mode_set(void);
void opmode_task_init(void);
void Opmode_Set(void);
void Opmode_test_mode(void);
bool feeder_mode_init(bool status, uint8_t mode);

#endif