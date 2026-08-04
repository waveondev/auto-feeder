#include "opmode_task.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "app_config_flash.h"
#include "app_TOF.h"
#include "app_led.h"
#include "app_HX711.h"
#include "ble_tracker_id.h"
#include "debug_cli.h"
#include <math.h>
#include "aws_iot_task.h"
#include "app_adc.h"
#include "app_acc_motor.h"
#include "app_slid_motor.h"
#include "app_feed_motor.h"
#include "app_sensor.h"
static QueueHandle_t opModeQueue = NULL;

static const char* TAG = __FILE__;
#define OPMODE_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
static uint32_t current_opmode = FEED_MODE_SCHEDULED_PORTION;
static esp_timer_handle_t opmode_timer = NULL;
static esp_timer_handle_t Slid_closed_timer = NULL;
void Slid_Timer_Set(bool state,uint32_t timeout);
// 1초 뒤 타이머가 만료되면 실행될 콜백 함수
    // 2) US 상수 매크로 형태 (60초 = 60 * 1초)
#define SEC_TO_US(sec) ((uint64_t)(sec) * 1000000ULL)
static float start_weight = 0;
static float actual_weight_diff = 0;
static void opmode_timer_callback(void* arg)
{
    //ESP_LOGI(TAG, "3초 동안 추가 입력이 없어 현재 모드로 확정합니다: %d", current_opmode);
    app_nvs_save_set();
        water_fault_enable(WATER_MODECHANGE);
    water_fault_disable(WATER_MODECHANGE);
    // TODO: 여기에 모드가 최종 확정되었을 때 실행할 동작(예: 화면 갱신, 실제 하드웨어 제어 등)을 넣으세요.
}

void Opmode_test_mode(void)
{
    current_opmode = OP_MODE_TEST;
}
void Opmode_Set(void)
{
    app_config_t* app_config = get_app_config();
    switch(current_opmode)
    {
        case FEED_MODE_SCHEDULED_PORTION:
            current_opmode = FEED_MODE_MANUAL_PORTION;
        break;
        case FEED_MODE_MANUAL_PORTION:
            current_opmode = FEED_MODE_FREE_FEEDING;
        break;
        case FEED_MODE_FREE_FEEDING:
        default:
            current_opmode = FEED_MODE_SCHEDULED_PORTION;
        break;
    }
    app_config->op_mode = current_opmode;
    Slid_Timer_Set(false, 0);
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

        esp_timer_start_once(opmode_timer, 5000000);

        ESP_LOGI(TAG, "모드 변경됨 -> %d (10초 타이머 시작/리셋)", current_opmode);
    }
}



static void Slid_closed_timer_callback(void* arg)
{

    if(Sliding_Front_Enable() || !Sliding_Back_Enable())
    {
        if(!VL53L0X_Detect())
        {
            Sliding_CCW(100);
        }
        else
        {
            // 장애물이나 감지가 계속되면 60초 뒤에 다시 체크하도록 타이머 재가동
            // (콜백 안에서는 esp_timer_start_once를 직접 쓰는 것이 안전합니다)
            esp_timer_start_once(Slid_closed_timer, SEC_TO_US(60));
            ESP_LOGI(TAG, "VL53L0X 감지됨 -> 60초 후 재시도 타이머 설정");
        }
    }
    if(start_weight != 0.0f && Sliding_Back_Enable())
    {
        // 1. 현재 로드셀 값(남은양/마지막값) 가져오기
        float current_end_weight = loadcell_data_get();
        
        // 2. 먹은양 = 사료 넣은 무게(diff) - 현재 로드셀 값
        float current_eat_weight = actual_weight_diff - current_end_weight;

        // 3. 로그 출력 (시작값, 넣은양(diff), 남은양, 먹은양 순서)
        ESP_LOGI(TAG, "start_weight = %.2f | diff = %.2f | 남은양 = %.2f | 먹은양 = %.2f", 
                start_weight, actual_weight_diff, current_end_weight, current_eat_weight);        

        // 4. 변수 초기화
        start_weight = 0.0f;
        actual_weight_diff = 0.0f;
    }
        
}

void Slid_Timer_Set(bool state,uint32_t timeout)
{
    // 2. 타이머가 처음 호출된 거라면 타이머를 생성
    if (Slid_closed_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &Slid_closed_timer_callback,
            .name = "Slid_closed_timer_timer"
        };
        esp_timer_create(&timer_args, &Slid_closed_timer);
    }
    else {
        // 💡 이미 타이머가 존재한다는 뜻은, 이전에 버튼을 누른 적이 있다는 것!
        // 즉, 1초 이내에 다시 들어왔을 확률이 높으므로 기존 타이머를 멈춤.
        esp_timer_stop(Slid_closed_timer);
    }

    if(state)
    {
        esp_timer_start_once(Slid_closed_timer, SEC_TO_US(timeout));
        ESP_LOGI(TAG, "SlidTimer set"); 
    }
    else
        ESP_LOGI(TAG, "SlidTimer reset"); 



   
}
static bool feeder_mode_flag = false;


typedef enum{
    SLID_IN = 0,
    SLID_IN_ING,
    SLID_IN_END,
    FEED_ING,
    FEED_END,
    SLID_OUT_ING,
    SLID_OUT_END
}FeederState_e;
static FeederState_e FeederState = SLID_IN;
void feeder_mode_init(void)
{
    if(feeder_mode_flag == false)
    {
        FeederState = SLID_IN;
        feeder_mode_flag = true;
        if (esp_timer_is_active(Slid_closed_timer)) {
            esp_timer_stop(Slid_closed_timer);
        } 
    }
}

void keep_sliding(void)
{
    Sliding_CW(100);
    Slid_Timer_Set(true, 10);      
}

static void Opmode_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting Opmode_task");
    app_config_t* app_config = get_app_config();

    
    
    while (1) {
        bool sensor_detected = VL53L0X_Detect();
        uint32_t current_tick = xTaskGetTickCount();

        DBG_Resister_t* DBG_Resister = Debug_Get();
        if(DBG_Resister->motor)
        {

        }
        else
        {
            if(feeder_mode_flag)
            {
                switch(FeederState)
                {
                    case SLID_IN :
                        if(!Sliding_Back_Enable())
                            Sliding_CCW(100);
                        FeederState = SLID_IN_ING;
                    break;
                    case SLID_IN_ING :
                        if(GetSlid_ADC() > 1000)
                        {
                            Sliding_coast(); 
                        }
                        if(Sliding_Back_Enable())
                        {
                            FeederState = SLID_IN_END;
                        }
                    break;
                    case SLID_IN_END :
                        vTaskDelay(1000);
                        start_weight = loadcell_data_get();//초기 로드셀 무게
                        Feeder_CW();
                        FeederState = FEED_ING;
                    break;
                    case FEED_ING :
                        if(GetFeed_ADC() > 500)
                        {
                            Feeder_coast(); 
                        }
                        while(1)
                        {
                            // 1. 현재 로드셀 값 읽기
                            if(GetFeed_ADC() > 500)
                            {
                                Feeder_CCW(); 
                                vTaskDelay(300);
                                Feeder_CW(); 
                            }
                            
                            // 2. 초기 무게 대비 변화량을 양수로 계산 
                            actual_weight_diff = loadcell_data_get() - start_weight;
                            ESP_LOGI(TAG,"Start: %.2f | Current: %.2f | Diff: %.2f\r\n", start_weight, loadcell_data_get(), actual_weight_diff);
                            // ADC 조건 또는 로드셀 차이 조건 만족 시 피더 정지 및 상태 전환
                            float gram = (float)app_config->dispense_amount_g;
                            if(actual_weight_diff >= gram)
                            {
                                Feeder_coast(); 
                                FeederState = FEED_END; // 조건 만족 시 FEED_END로 변경
                                break;
                            }
                            vTaskDelay(50);
                        }
                    break;
                    case FEED_END :
                        actual_weight_diff = loadcell_data_get() - start_weight;
                        Sliding_CW(100);
                        FeederState = SLID_OUT_ING;
                    break;
                    case SLID_OUT_ING :
                        if(GetSlid_ADC() > 1000)
                        {
                            Sliding_coast(); 
                        }
                        if(Sliding_Front_Enable())
                        {
                            FeederState = SLID_OUT_END;
                        }
                    break;
                    case SLID_OUT_END :
                        feeder_mode_flag = false;
                    break;
                }
            }
            else
            {
                switch(current_opmode)
                {
                    case FEED_MODE_SCHEDULED_PORTION:
                        // 1. 현재 타이머가 돌고 있는지 체크하는 방법
                        if(Sliding_Front_Enable())
                        {
                            if (!esp_timer_is_active(Slid_closed_timer)) {
                                Slid_Timer_Set(true, 60);
                            }                             
                        }
                    break;
                    case FEED_MODE_MANUAL_PORTION:
                        if(VL53L0X_Detect())
                        {
                            if (esp_timer_is_active(Slid_closed_timer)) {
                                esp_timer_stop(Slid_closed_timer);
                            } 
                            Sliding_CW(100);
                        }
                        else
                        {
                            if (!esp_timer_is_active(Slid_closed_timer)) {
                                Slid_Timer_Set(true, 60);
                            } 
                        }
                    break;
                    case FEED_MODE_FREE_FEEDING:
                        
                    break;
                    default:
                    break;
                }
            }
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}





void opmode_task_init(void)
{
    app_config_t* app_config = get_app_config();
    current_opmode = app_config->op_mode;
    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            Opmode_task,                  // 태스크 함수
            "opmode_task",                // 태스크 이름
            OPMODE_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 2,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating Button_task on Core 1");
    }
}


