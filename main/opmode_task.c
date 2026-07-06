#include "opmode_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "app_config_flash.h"
#include "esp_timer.h"
#include "app_moter.h"
#include "app_TOF.h"
#include "app_led.h"
#include "app_HX711.h"
#include "ble_tracker_id.h"
static QueueHandle_t opModeQueue = NULL;

static const char* TAG = __FILE__;
#define OPMODE_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 3)
static uint32_t current_opmode = OP_MODE_NORMAL;
static esp_timer_handle_t opmode_timer = NULL;

// 1초 뒤 타이머가 만료되면 실행될 콜백 함수
static void opmode_timer_callback(void* arg)
{
    //ESP_LOGI(TAG, "3초 동안 추가 입력이 없어 현재 모드로 확정합니다: %d", current_opmode);
    app_nvs_save_set();
    // TODO: 여기에 모드가 최종 확정되었을 때 실행할 동작(예: 화면 갱신, 실제 하드웨어 제어 등)을 넣으세요.
}

void Opmode_test_mode(void)
{
    current_opmode = OP_MODE_TEST;
}
void Opmode_Set(void)
{
        app_config_t* app_config = get_app_config();
        //start_motor_with_boost(0,0);
        switch(current_opmode)
        {
            case OP_MODE_NORMAL:
                current_opmode = OP_MODE_NIGHT;
            break;
            case OP_MODE_NIGHT:
                current_opmode = OP_MODE_SMART;
            break;
            case OP_MODE_SMART:
                current_opmode = OP_MODE_SLEEP;
            break;
            default:
                current_opmode = OP_MODE_NORMAL;
            break;
        }
        app_config->op_mode = current_opmode;
    {
        // 2. 타이머가 처음 호출된 거라면 타이머를 생성
        if (opmode_timer == NULL) {
            const esp_timer_create_args_t timer_args = {
                .callback = &opmode_timer_callback,
                .name = "opmode_delay_timer"
            };
            esp_timer_create(&timer_args, &opmode_timer);
        }
        else {
            // 💡 이미 타이머가 존재한다는 뜻은, 이전에 버튼을 누른 적이 있다는 것!
            // 즉, 1초 이내에 다시 들어왔을 확률이 높으므로 기존 타이머를 멈춤.
            esp_timer_stop(opmode_timer);
        }
    // 4. 타이머를 1초(1,000,000 마이크로초)로 다시 시작
        esp_timer_start_once(opmode_timer, 5000000);

        ESP_LOGI(TAG, "모드 변경됨 -> %d (10초 타이머 시작/리셋)", current_opmode);
    }

}


static smart_state_t smart_state = SMART_IDLE;
smart_state_t Time_ratio_state(void)
{
    return smart_state;
}

#if 1
static void Opmode_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting Opmode_task");
    


    static uint32_t smart_timer_target = 0; // 각 상태별 마감 시한 틱 저장
    float start_weight = 0;
    while (1) {
        bool sensor_detected = VL53L0X_Detect();
        uint32_t current_tick = xTaskGetTickCount();
        switch (smart_state)
        {
            case SMART_IDLE:
                
                if (sensor_detected) 
                {
                    // 💡 1. 센서 감지 즉시 모터 가동! 그리고 5초 검증 타이머 시작
                    start_weight = loadcell_data_get();
                    smart_timer_target = current_tick + (5000 / portTICK_PERIOD_MS);
                    smart_state = SMART_RUN_VERIFY;
                    ESP_LOGI(TAG, "SMART: Sensor detected! Motor ON immediately. Verifying 5s...");
                }
                break;

            case SMART_RUN_VERIFY:
                // 5초가 가기 전에 센서가 끊기면 칼같이 끄고 대기 상태로 복귀
                if (!sensor_detected) 
                {
                    smart_state = SMART_IDLE;
                    ESP_LOGI(TAG, "SMART: Sensor lost before 5s. Motor STOPPED.");
                    break;
                }

                // 5초 동안 센서가 짱짱하게 잘 버텼는지 확인
                if ((int32_t)(smart_timer_target - current_tick) <= 0) 
                {
                    smart_state = SMART_RUN_STABLE;
                    ESP_LOGI(TAG, "SMART: 5-second verification SUCCESS. Stable running...");
                }
                break;

            case SMART_RUN_STABLE:

                // 💡 2. 5초 이상 유지된 이후 센서 감지가 사라지면 "바로 끄기"
                if (!sensor_detected) 
                {
                    // 💡 3. 꺼진 후 3초 동안 계속 감지 안 되는지 체크 시작
                    smart_timer_target = current_tick + (3000 / portTICK_PERIOD_MS);
                    smart_state = SMART_STOP_CHECK;
                    ESP_LOGI(TAG, "SMART: Sensor lost. Motor STOPPED immediately. Checking 3s stable off...");
                }
                break;

            case SMART_STOP_CHECK:
                // ⚠️ 3초 버티는 도중에 센서가 다시 감지되었다면? -> 끄기 취소하고 다시 안정 구동으로!
                if (sensor_detected) 
                {
                    smart_state = SMART_RUN_STABLE;
                    ESP_LOGI(TAG, "SMART: Sensor came back during 3s check! Motor ON again.");
                    break;
                }

                // 💡 3초 동안 센서가 단 한 번도 들어오지 않고 완벽하게 꺼짐이 유지된 경우
                if ((int32_t)(smart_timer_target - current_tick) <= 0) 
                {
                    smart_state = SMART_IDLE; // 완전히 끝내고 대기 상태로 복귀
                    float diff_weight = start_weight - loadcell_data_get();
                    Tracker_waterintake_end((uint32_t)(diff_weight*100));
                    ESP_LOGI(TAG, "SMART: 3-second off confirmed. Cycle fully finished. %.2f",diff_weight );
                }
                break;
        }
        if(ota_enable() || hardware_error_enable())
        {
            start_motor_with_boost(0, 0);
        }
        else
        {
                switch(current_opmode)
                {
                    case OP_MODE_SMART:
                    {
                        if(sensor_detected || sense_enable())
                            start_motor_with_boost(100, 0);
                        else
                            start_motor_with_boost(0, 0);
                    }
                    break;
                    // 타 모드는 기본 구조 유지
                    case OP_MODE_NORMAL:
                        start_motor_with_boost(100, 0);
                        break;
                    case OP_MODE_NIGHT:
                        start_motor_with_boost(90, 0);
                        break;
                    case OP_MODE_SLEEP:
                        start_motor_with_boost(0, 0);
                        break;
                    default:
                        break;
                }
        }


        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
#else
static void Opmode_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting Opmode_task");
    uint32_t check_delay_ms = 5000; // 기본값 5초 (5000ms)
    // 💡 5초 뒤 센서 상태를 체크하기 위한 전역적 타이머 변수
    static uint32_t sensor_check_until = 0; 
    static bool is_checking = false;
    static bool sensor_processed = false; // 💡 한 번 처리된 센싱인지 확인하는 락(Lock) 플래그
    
    static uint32_t motor_must_run_until = 0; // SMART 모드용 20초 마감 시한
    static bool is_smart_running = false;     // SMART 모드 구동 플래그

    while (1) {
        bool sensor_detected = VL53L0X_Detect();
        uint32_t current_tick = xTaskGetTickCount();
        
        // -----------------------------------------------------------------
        // [5초 체크 타이머 로직] 모터 동작과 무관하게 독립적으로 작동
        // -----------------------------------------------------------------
        
        // 1. 센서가 감지되었고, 타이머도 안 돌고 있고, '이미 5초 체크를 끝낸 상태'도 아닐 때
        if (sensor_detected && !is_checking && !sensor_processed) 
        {
            sensor_check_until = current_tick + (check_delay_ms / portTICK_PERIOD_MS);
            is_checking = true;
            ESP_LOGI(TAG, "Sensor detected! Will check final status 5 seconds later...");
        }

        // 2. 5초 체크 예약이 걸려있는 상태에서 시간이 다 되었는지 감시
        if (is_checking) 
        {
            int32_t remaining_check_ticks = (int32_t)(sensor_check_until - current_tick);
            
            if (remaining_check_ticks <= 0) 
            {
                // ⚠️ 드디어 최초 감지 후 딱 5초가 지난 시점!
                if (sensor_detected) 
                {
                    // 5초 뒤에도 여전히 true인 경우 처리할 로직
                    ESP_LOGI(TAG, "[5초 체크 성공] 5초 뒤에도 여전히 센서가 감지 상태(TRUE)입니다.");
                } 
                else 
                {
                    // 5초 뒤에는 false로 바뀐 경우 처리할 로직
                    ESP_LOGI(TAG, "[5초 체크 실패] 5초 뒤 확인해보니 센서가 꺼져있습니다(FALSE).");
                }
                
                is_checking = false;  // 타이머 종료
                sensor_processed = true; // 💡 5초 체크를 끝냈으니 '처리 완료' 표시 (이후 5초마다 중복실행 방지)
            }
        }

        // 3. ⭐️ 센서가 완전히 사라졌을 때 (손을 뗐을 때) 모든 상태 리셋
        if (!sensor_detected)
        {
            sensor_processed = false; // 💡 다음 감지를 위해 락(Lock) 해제
            is_checking = false;   // 💡 5초 도달 전에 손을 떼면 돌고 있던 타이머도 즉시 취소
        }


        // -----------------------------------------------------------------
        // [기존 모터 제어 로직] 5초 타이머와 상관없이 조건 맞으면 바로 작동!
        // -----------------------------------------------------------------
        switch(current_opmode)
        {
            case OP_MODE_NORMAL:
                start_motor_with_boost(100, 0);
                break;

            case OP_MODE_NIGHT:
                start_motor_with_boost(40, 0);
                break;

            case OP_MODE_SMART:
            {
                // SMART 모드용 20초 보장 로직 (센서 들어오면 바로 시작)
                if (sensor_detected && !is_smart_running) 
                {
                    motor_must_run_until = current_tick + (20000 / portTICK_PERIOD_MS);
                    is_smart_running = true;
                    ESP_LOGI(TAG, "SMART Mode: Motor started immediately. 20s timer active.");
                }

                if (is_smart_running) 
                {
                    int32_t remaining_ticks = (int32_t)(motor_must_run_until - current_tick);

                    if (remaining_ticks <= 0) 
                    {
                        if (sensor_detected) {
                            start_motor_with_boost(100, 0);
                        } else {
                            start_motor_with_boost(0, 0);
                            is_smart_running = false;
                            ESP_LOGI(TAG, "SMART Mode: 20s passed & No sensor. Stopped.");
                        }
                    } 
                    else {
                        start_motor_with_boost(100, 0);
                    }
                } 
                else {
                    start_motor_with_boost(0, 0);
                }
                break;
            }

            case OP_MODE_SLEEP:
                //start_motor_with_boost(0, 0);
                break;

            default:
                break;
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
#endif





void opmode_task_init(void)
{
    TaskHandle_t xHandle = NULL;
    static uint8_t ucParameterToPass;
    app_config_t* app_config = get_app_config();
    current_opmode = app_config->op_mode;
    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            Opmode_task,                  // 태스크 함수
            "opmode_task",                // 태스크 이름
            OPMODE_TASK_STACK_SIZE,       // 스택 크기
            &ucParameterToPass,        // 파라미터
            tskIDLE_PRIORITY + 2,      // 우선순위
            &xHandle,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating Button_task on Core 1");
    }
}


