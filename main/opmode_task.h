#ifndef __OPMODE_TASK_H__
#define __OPMODE_TASK_H__


typedef enum {
    OP_MODE_NORMAL = 0,
    OP_MODE_NIGHT,
    OP_MODE_SMART,
    OP_MODE_SLEEP,
    OP_MODE_TEST
} op_mode_e;

// SMART 모드 내부 상태 머신 정의
typedef enum {
    SMART_IDLE,          // 대기 상태 (센서 없음, 모터 Off)
    SMART_RUN_VERIFY,    // 즉시 구동 후 5초 유지 검증 상태 (모터 On)
    SMART_RUN_STABLE,    // 5초 검증 통과 후 안정 구동 상태 (모터 On)
    SMART_STOP_CHECK     // 모터는 껐지만, 3초 동안 계속 센서가 없는지 감시하는 상태 (모터 Off)
} smart_state_t;

void opmode_task_init(void);
void Opmode_Set(void);
void Opmode_test_mode(void);
smart_state_t Time_ratio_state(void);

#endif