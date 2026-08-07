#ifndef __OPMODE_TASK_H__
#define __OPMODE_TASK_H__

#include "esp_log.h"
typedef enum {
    FEED_MODE_SCHEDULED_PORTION = 0, // 정량/정시 배식
    FEED_MODE_MANUAL_PORTION    = 1, // 정량 배식 (수동/즉시)
    FEED_MODE_FREE_FEEDING      = 2, // 자율 배식
    OP_MODE_TEST                = 3
} op_mode_e;

void Clean_mode_set(void);
void opmode_task_init(void);
void Opmode_Set(void);
void Opmode_test_mode(void);
bool feeder_mode_init(void);

#endif