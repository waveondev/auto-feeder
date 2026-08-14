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
#include "isd2360.h"
static QueueHandle_t opModeQueue = NULL;

static const char* TAG = __FILE__;
#define OPMODE_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
static uint32_t current_opmode = FEED_MODE_SCHEDULED_PORTION;
static esp_timer_handle_t opmode_timer = NULL;
static esp_timer_handle_t Slid_closed_timer = NULL;
static esp_timer_handle_t Slid_weight_timer = NULL;
static esp_timer_handle_t food_dispense_timer = NULL;
static esp_timer_handle_t food_feed_timer = NULL;
void Slid_Close_Timer_Set(bool state,uint32_t timeout,int line);
void Slid_weight_Timer_Set(bool state,uint32_t timeout);
// 1초 뒤 타이머가 만료되면 실행될 콜백 함수
    // 2) US 상수 매크로 형태 (60초 = 60 * 1초)
#define SEC_TO_US(sec) ((uint64_t)(sec) * 1000000ULL)
#define SEC_TO_MS(sec) ((uint64_t)(sec) * 1000ULL)
static float start_weight = 0;
static float actual_weight_diff = 0;
static bool feeder_mode_flag = false;
static bool diff_enable = false;
#define MOTOR_DEFAULT_TIME 20
void FoodDispense_Timer_Set(bool state,uint32_t timeout);

typedef enum{
    SLID_IN = 0,
    SLID_IN_ING,
    SLID_IN_END,
    FEED_ING,
    FEED_END,
    FEED_FAIL,    
}FeederState_e;
static FeederState_e FeederState = SLID_IN;

static void opmode_timer_callback(void* arg)
{
    //ESP_LOGI(TAG, "3초 동안 추가 입력이 없어 현재 모드로 확정합니다: %d", current_opmode);
    app_nvs_save_set();
    // TODO: 여기에 모드가 최종 확정되었을 때 실행할 동작(예: 화면 갱신, 실제 하드웨어 제어 등)을 넣으세요.
}

static bool Clean_state = false;
static bool Clean_enable = false;
void Clean_mode_set(void)
{
    if(Clean_enable == true)
        return;
    Clean_state = !Clean_state;
    Clean_enable = true;

}


void CleanMode(void)
{
    if(Clean_enable == false)
        return;
    Clean_enable = false;
    feeder_mode_flag = false;
    Slid_Close_Timer_Set(false, 0,__LINE__);
    if(Feed_Front_Enable() == false)
    {
        Feeder_CCW();         
        while(Feed_Front_Enable() == false)
        {
            vTaskDelay(10);
        }
    }


    if(Clean_state == true)
    {
        Sliding_CW(100);
        while(Sliding_Back_Enable() == true)
        {
            vTaskDelay(10);
        }
        Slid_Close_Timer_Set(true, SEC_TO_MS(60),__LINE__);
    }
    else
        Sliding_CCW(100);

}
void Opmode_test_mode(void)
{
    current_opmode = OP_MODE_TEST;
}
void Opmode_Set(void)
{
    app_config_t* app_config = get_app_config();
    if(feeder_mode_flag)
        return;
    switch(current_opmode)
    {
        case FEED_MODE_SCHEDULED_PORTION:
            current_opmode = FEED_MODE_MANUAL_PORTION;
            FoodDispense_Timer_Set(true, (app_config->dispense_duration * 60)*1000);
        break;
        case FEED_MODE_MANUAL_PORTION:
            current_opmode = FEED_MODE_FREE_FEEDING;
            FoodDispense_Timer_Set(false, 0);
        break;
        case FEED_MODE_FREE_FEEDING:
        default:
            current_opmode = FEED_MODE_SCHEDULED_PORTION;
            FoodDispense_Timer_Set(true, (app_config->dispense_duration * 60)*1000);
        break;
    }
    app_config->op_mode = current_opmode;
    if(!Sliding_Back_Enable())
        Slid_Close_Timer_Set(true, SEC_TO_MS(10),__LINE__);
    {
        esp_timer_stop(opmode_timer);
        esp_timer_start_once(opmode_timer, 5000000);

        ESP_LOGI(TAG, "모드 변경됨 -> %d (10초 타이머 시작/리셋)", current_opmode);
    }
}



static void Slid_closed_timer_callback(void* arg)
{
    app_config_t* app_config = get_app_config();

    if(app_config->sliding_close_mode)
    {
        if(loadcell_data_get() > 5.0f)
        {
            Slid_Close_Timer_Set(true, SEC_TO_MS(60),__LINE__);
            ESP_LOGI(TAG, "잔여물 확인됨 ");
            return;
        }        
    }

    if(Sliding_Front_Enable() || !Sliding_Back_Enable())
    {
        if(!VL53L0X_Detect(true))
        {

            Sliding_CCW(100);
            if(start_weight != 0)
                Slid_weight_Timer_Set(true,SEC_TO_MS(60));
        }
        else
        {
            feeder_fault_enable(MOTOR_SLIDING_BLOCKED,true);
            Slid_Close_Timer_Set(true, SEC_TO_MS(60),__LINE__);
            ESP_LOGI(TAG, "VL53L0X 감지됨 -> 60초 후 재시도 타이머 설정");
        }
    }    
}

static void Slid_weight_timer_callback(void* arg)
{
    if(start_weight != 0)
    {
        if(Sliding_Back_Enable())
        {
            start_acc_motor_with_boost();
            ESP_LOGI(TAG,"start_weight = %.2f now = %.2f",start_weight, loadcell_data_get());

            float diff_weight = start_weight - loadcell_data_get();

            if(diff_weight > 1.0f)
            {
                Tracker_waterintake_end((uint32_t)(diff_weight));
                mqtt_queue_send(MESSEGE_INTAKE);
            }
            else
            {   
                if(diff_weight < -50.0f)
                {
                    ESP_LOGI(TAG, "잔여물 증가 ");
                    feeder_fault_enable(WEIGHT_ABNORMAL_INCREASE,true);
                }
            }
            start_weight = 0.0f;
        }
        else
        {
            if (!esp_timer_is_active(Slid_weight_timer)) {
                Slid_weight_Timer_Set(true, SEC_TO_MS(MOTOR_DEFAULT_TIME));
            } 
        }
    }

}
static uint8_t fail_count = 0;
static void food_timer_callback(void* arg)
{

    bool ret = feeder_mode_init();
    if(ret == true)
        fail_count = 0;
    else
    {
        FoodDispense_Timer_Set(true, 60000);
        fail_count++;
    }
        

    if(fail_count > 3)
    {
        ESP_LOGI(TAG,"FAILER FEED");   
    }
}

static void food_feed_callback(void* arg)
{
    FeederState = FEED_FAIL;
}

void Slid_Close_Timer_Set(bool state,uint32_t timeout,int line)
{
    if (esp_timer_is_active(Slid_closed_timer)) {
        esp_timer_stop(Slid_closed_timer);
    } 

    if(state)
    {
        esp_timer_start_once(Slid_closed_timer, SEC_TO_MS(timeout));
        ESP_LOGI(TAG, "Slid close set %d", line); 
    }
}

#if 1
void Slid_weight_Timer_Set(bool state,uint32_t timeout)
{
    if (esp_timer_is_active(Slid_weight_timer)) {
        esp_timer_stop(Slid_weight_timer);
    } 
    if(state)
    {
        esp_timer_start_once(Slid_weight_timer, SEC_TO_MS(timeout));
        ESP_LOGI(TAG, "weight set"); 
    }
    else
        ESP_LOGI(TAG, "weight reset"); 
}

#endif

void FoodDispense_Timer_Set(bool state,uint32_t timeout)
{
    #if 1
    if (esp_timer_is_active(food_dispense_timer)) {
        esp_timer_stop(food_dispense_timer);
    } 
    if(state)
    {
        esp_timer_start_once(food_dispense_timer, SEC_TO_MS(timeout));
        ESP_LOGI(TAG, "food set"); 
    }
    else
        ESP_LOGI(TAG, "food reset"); 
    #endif
}

void FoodFeed_Timer_Set(bool state,uint32_t timeout)
{
    #if 1
    if (esp_timer_is_active(food_feed_timer)) {
        esp_timer_stop(food_feed_timer);
    } 
    if(state)
    {
        esp_timer_start_once(food_feed_timer, SEC_TO_MS(timeout));
        ESP_LOGI(TAG, "food set"); 
    }
    else
        ESP_LOGI(TAG, "food reset"); 
    #endif
}


bool feeder_mode_init(void)
{
    if(VL53L0X_Detect(false) && !Sliding_Back_Enable())
    {
        return false;
    }
    if(hardware_error_enable())
        return false;
    if(feeder_mode_flag == false)
    {
        Slid_Close_Timer_Set(false, 0,__LINE__);
        Slid_weight_Timer_Set(false,0);
        FeederState = SLID_IN;
        feeder_mode_flag = true;

        return true;
    }
    return false;
}

void Open_Slid(void)
{
    bool back_enable = false;
    if(Sliding_Front_Enable())
        return;

    Slid_Close_Timer_Set(true, SEC_TO_MS(60),__LINE__);
    Slid_weight_Timer_Set(false,0);

    while(Sliding_Back_Enable() == false)
    {
        Sliding_CCW(100);
        vTaskDelay(pdMS_TO_TICKS(1000)); 
        back_enable = true;
        ESP_LOGI(TAG,"SLID BACK WAIT");
    }  
    if(back_enable == true)
        vTaskDelay(3000);
    if(Sliding_Back_Enable())
    {
    
        start_weight = loadcell_data_get();
        vTaskDelay(1000);
        Sliding_CW(100);
        while(Sliding_Front_Enable() == false)
        {
            vTaskDelay(1000);
        }
    }

}

// SMART 모드 내부 상태 머신 정의
typedef enum {
    SMART_IDLE,          // 대기 상태 (센서 없음, 모터 Off)
    SMART_RUN_VERIFY,    // 즉시 구동 후 5초 유지 검증 상태 (모터 On)
    SMART_RUN_STABLE,    // 5초 검증 통과 후 안정 구동 상태 (모터 On)
} smart_state_t;
static smart_state_t smart_state = SMART_IDLE;
void Smart_Feeder(void)
{
    static uint32_t smart_timer_target = 0; // 각 상태별 마감 시한 틱 저장

    bool sensor_detected = VL53L0X_Detect(true);
    uint32_t current_tick = xTaskGetTickCount();

    switch (smart_state)
    {
        case SMART_IDLE:
            if (sensor_detected) 
            {
                smart_timer_target = current_tick + ((5*1000) / portTICK_PERIOD_MS);
                smart_state = SMART_RUN_VERIFY;
            }
        break;

        case SMART_RUN_VERIFY:
            if (!sensor_detected) 
            {
                smart_state = SMART_IDLE;
                ESP_LOGI(TAG, "SMART: Sensor lost before 5s. Motor STOPPED.");
                break;
            }

            // 3. 5초 동안 센서가 짱짱하게 잘 버텼는지 확인
            if ((int32_t)(smart_timer_target - current_tick) <= 0) 
            {
                // 5초 동안 급격하게 튀지 않고 무사히 통과 완료!
                mqtt_queue_send(MESSEGE_ACCESS);
                smart_state = SMART_RUN_STABLE;
                ESP_LOGI(TAG, "SMART: 5-second verification SUCCESS. Stable running...");
            }
        break;

        case SMART_RUN_STABLE:
            if (!sensor_detected) 
            {
                smart_state = SMART_IDLE;
            }
        break;
    }
}

static void Opmode_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting Opmode_task");
    app_config_t* app_config = get_app_config();

    
    float feed_start = 0;
    float feed_diff = 0;
    uint8_t Food_Max = 0;
    uint8_t Food_Empty_count = 0;
    while (1) {
        uint32_t current_tick = xTaskGetTickCount();

        DBG_Resister_t* DBG_Resister = Debug_Get();
        if(DBG_Resister->motor)
        {

        }
        else
        {
            Smart_Feeder();
            CleanMode();
            if(feeder_mode_flag)
            {
                switch(FeederState)
                {
                    case SLID_IN :
                        Food_Max = 0;

                        if(!Sliding_Back_Enable())
                            Sliding_CCW(100);
                        FeederState = SLID_IN_ING;
                    break;
                    case SLID_IN_ING :
                        if(Sliding_Back_Enable())
                        {
                            FeederState = SLID_IN_END;
                            vTaskDelay(pdMS_TO_TICKS(3000)); 
                        }
                    break;
                    case SLID_IN_END :
                        feed_start = loadcell_data_get();//초기 로드셀 무게
                        if(current_opmode == FEED_MODE_FREE_FEEDING)
                            feed_start = 0.0f;
                            
                        Feeder_CW();

                        if(Food_Detected_State())
                        {
                            feeder_fault_enable(FOOD_LOW,true);
                            led_bit_enable(FOOD_LOW_BIT);
                        }
                        else
                            led_bit_disable(FOOD_LOW_BIT);             

                        FoodFeed_Timer_Set(true,120000);
                        FeederState = FEED_ING;
                    break;
                    case FEED_ING :                        
                            if(Food_Door_Detected_State())
                            {
                                Food_Max++;
                            }
                            else
                            {
                                Food_Max = 0;
                            }

                            if(Food_Max != 0 && (Food_Max % 10) == 0)
                            {
                                FoodFeed_Timer_Set(true,120000);
                                Feeder_coast();
                                for(int i=0;i<5;i++)
                                {
                                    Sliding_CW(100);
                                    while(Sliding_Back_Enable() == true)
                                    {
                                        vTaskDelay(10);
                                    }
                                    vTaskDelay(300);
                                    Sliding_CCW(100);
                                    while(Sliding_Back_Enable() == false)
                                    {
                                        vTaskDelay(10);
                                    }
                                }
                                vTaskDelay(1000);
                                Feeder_CW();
                                ESP_LOGI(TAG,"Food_Max = %d",Food_Max);
                            }
                            feed_diff = loadcell_data_get() - feed_start;                   

                            float gram = (float)app_config->dispense_amount_g;
                            if(feed_diff >= gram-10)
                            {
                                Feeder_coast();
                                vTaskDelay(2000);
                                feed_diff = loadcell_data_get() - feed_start;
                                if(feed_diff >= gram)
                                {
                                    FoodFeed_Timer_Set(false,0);
                                    ESP_LOGI(TAG,"Start: %.2f | Current: %.2f | Diff: %.2f\r\n", feed_start, loadcell_data_get(), feed_diff);
                                    FeederState = FEED_END; // 조건 만족 시 FEED_END로 변경
                                    break;
                                }                    
                                else
                                {
                                    Feeder_CW(); 
                                    vTaskDelay(200);  
                                }    
                            }                            

                    break;
                    case FEED_END :
  
                        mqtt_queue_send(MESSEGE_DISPENSE);
                        Feeder_CCW();         
                        while(Feed_Front_Enable() == false)
                        {
                            vTaskDelay(10);
                        }
                        feeder_mode_flag = false;           
                        if(current_opmode == FEED_MODE_SCHEDULED_PORTION)
                            Open_Slid();
                    break;
                    case FEED_FAIL :
                        led_bit_enable(FOOD_EMPTY_BIT);
                        feeder_fault_enable(FOOD_EMPTY,true);
                        Feeder_coast();
                        feeder_mode_flag = false;
                    break;
                }
            }
            else
            {
                switch(current_opmode)
                {
                    case FEED_MODE_SCHEDULED_PORTION:

                    break;
                    case FEED_MODE_MANUAL_PORTION:
                        if(VL53L0X_Detect(true))
                        {
                            Open_Slid();
                        }
                    break;
                    case FEED_MODE_FREE_FEEDING:
                        float gram = (float)app_config->dispense_amount_g;
                        if(Sliding_Back_Enable() && (loadcell_data_get() < gram))
                        {
                            feeder_mode_init();
                        }
                        else
                        {
                            if(VL53L0X_Detect(true))
                            {
                                Open_Slid();
                            }
                        }

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

    const esp_timer_create_args_t slid_weight_args = {
        .callback = &Slid_weight_timer_callback,
        .name = "Slid_weight_timer_timer"
    };
    esp_timer_create(&slid_weight_args, &Slid_weight_timer);

    const esp_timer_create_args_t slid_closed_args = {
        .callback = &Slid_closed_timer_callback,
        .name = "Slid_closed_timer_timer"
    };
    esp_timer_create(&slid_closed_args, &Slid_closed_timer);

    const esp_timer_create_args_t optimer_args = {
        .callback = &opmode_timer_callback,
        .name = "opmode_delay_timer"
    };
    esp_timer_create(&optimer_args, &opmode_timer);


    const esp_timer_create_args_t food_dispense_args = {
        .callback = &food_timer_callback,
        .name = "food_delay_timer"
    };
    esp_timer_create(&food_dispense_args, &food_dispense_timer);

    const esp_timer_create_args_t food_feed_args = {
        .callback = &food_feed_callback,
        .name = "food_feed_timer"
    };
    esp_timer_create(&food_feed_args, &food_feed_timer);
    
    #if 1
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
        #endif
}


