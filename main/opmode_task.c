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

void Slid_Close_Timer_Set(bool state,uint32_t timeout);
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


static void opmode_timer_callback(void* arg)
{
    //ESP_LOGI(TAG, "3초 동안 추가 입력이 없어 현재 모드로 확정합니다: %d", current_opmode);
    app_nvs_save_set();
        water_fault_enable(WATER_MODECHANGE);
    water_fault_disable(WATER_MODECHANGE);
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
    Slid_Close_Timer_Set(false, 0);
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
        Slid_Close_Timer_Set(true, 600000);
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
    Slid_Close_Timer_Set(false, 0);
    {
        esp_timer_stop(opmode_timer);
        esp_timer_start_once(opmode_timer, 5000000);

        ESP_LOGI(TAG, "모드 변경됨 -> %d (10초 타이머 시작/리셋)", current_opmode);
    }
}



static void Slid_closed_timer_callback(void* arg)
{
    app_config_t* app_config = get_app_config();

    if(Sliding_Front_Enable() || !Sliding_Back_Enable())
    {
        if(!VL53L0X_Detect())
        {
            Sliding_CCW(100);
            if(start_weight != 0)
                Slid_weight_Timer_Set(true,60000);
        }
        else
        {
            Slid_Close_Timer_Set(true, 60000);
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
            start_weight = 0.0f;
        }
        else
        {
            if (!esp_timer_is_active(Slid_weight_timer)) {
                Slid_weight_Timer_Set(true, MOTOR_DEFAULT_TIME*1000);
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

void Slid_Close_Timer_Set(bool state,uint32_t timeout)
{
    if (esp_timer_is_active(Slid_closed_timer)) {
        esp_timer_stop(Slid_closed_timer);
    } 

    if(state & !Sliding_Back_Enable())
    {
        esp_timer_start_once(Slid_closed_timer, SEC_TO_MS(timeout));
        ESP_LOGI(TAG, "Slid close set"); 
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

typedef enum{
    SLID_IN = 0,
    SLID_IN_ING,
    SLID_IN_END,
    FEED_ING,
    FEED_END,
    FEED_FAIL,    
}FeederState_e;
static FeederState_e FeederState = SLID_IN;
bool feeder_mode_init(void)
{
    if(VL53L0X_Detect() && !Sliding_Back_Enable())
    {
        return false;
    }
    if(food_empty_enable() || hardware_error_enable())
        return false;
    if(feeder_mode_flag == false)
    {
        Slid_Close_Timer_Set(false, 0);
        FeederState = SLID_IN;
        feeder_mode_flag = true;
        if (esp_timer_is_active(Slid_closed_timer)) {
            esp_timer_stop(Slid_closed_timer);
        } 
        return true;
    }
    return false;
}

void Open_Slid(void)
{
    bool back_enable = false;
    if(Sliding_Front_Enable())
        return;
    if (esp_timer_is_active(Slid_weight_timer)) {
        esp_timer_stop(Slid_weight_timer);
    } 
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

static void Opmode_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting Opmode_task");
    app_config_t* app_config = get_app_config();

    
    float feed_start = 0;
    float feed_diff = 0;
    uint8_t Food_Max = 0;
    uint8_t Food_Empty_count = 0;
    while (1) {
        bool sensor_detected = VL53L0X_Detect();
        uint32_t current_tick = xTaskGetTickCount();

        DBG_Resister_t* DBG_Resister = Debug_Get();
        if(DBG_Resister->motor)
        {

        }
        else
        {
            CleanMode();
            if(feeder_mode_flag)
            {
                switch(FeederState)
                {
                    case SLID_IN :
                        Food_Max = 0;
                        Food_Empty_count = 0;
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
                        if(food_empty_enable())
                            FeederState = FEED_FAIL;
                        else
                            FeederState = FEED_ING;
                    break;
                    case FEED_ING :                        
                            if(Food_Door_Detected_State())
                            {
                                Food_Max++;
                            }
                            else
                                Food_Max = 0;
                            if(Food_Max != 0 && (Food_Max % 10) == 0)
                            {
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
                            if(food_empty_enable())
                            {
                                Food_Empty_count++;
                                if(Food_Empty_count >= 100)
                                    FeederState = FEED_END;
                            }
                            else{
                                Food_Empty_count = 0;
                            }                                    

                            float gram = (float)app_config->dispense_amount_g;
                            if(feed_diff >= gram-10)
                            {
                                Feeder_coast();
                                vTaskDelay(1000);
                                feed_diff = loadcell_data_get() - feed_start;
                                if(feed_diff >= gram)
                                {
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
                        //Open_Slid();
                        {
                            if (!esp_timer_is_active(Slid_closed_timer)) {
                                Slid_Close_Timer_Set(true, MOTOR_DEFAULT_TIME*1000);
                            } 
                        }
                    break;
                    case FEED_MODE_MANUAL_PORTION:
                        if(VL53L0X_Detect())
                        {
                            Open_Slid();
                        }
                        else
                        {
                            if (!esp_timer_is_active(Slid_closed_timer)) {
                                Slid_Close_Timer_Set(true, MOTOR_DEFAULT_TIME*1000);
                            } 
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
                            if(VL53L0X_Detect())
                            {
                                Open_Slid();
                            }
                            else
                            {
                                if (!esp_timer_is_active(Slid_closed_timer)) {
                                    Slid_Close_Timer_Set(true, MOTOR_DEFAULT_TIME*1000);
                                } 
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


