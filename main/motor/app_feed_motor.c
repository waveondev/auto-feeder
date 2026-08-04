
#include <stdio.h>
#include "app_feed_motor.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_util.h"
#include "esp_log.h"
#include "debug_cli.h"
#include "app_config_flash.h"
#include "tx_mqtt.h"
#include "aws_iot_task.h"
#include "app_sensor.h"
#include "app_slid_motor.h"
#include "app_HX711.h"
#include "app_TOF.h"
#include "app_acc_motor.h"
static const char *TAG = __FILE__;

static QueueHandle_t feed_motor_queue = NULL;

void Feeder_CW(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(FEED_PWM_CW, 0);       
    gpio_set_level(FEED_PWM_IN, 1);
}
void Feeder_CCW(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(FEED_PWM_CW, 1);       
    gpio_set_level(FEED_PWM_IN, 1);
}

void Feeder_coast(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(FEED_PWM_CW, 1);       
    gpio_set_level(FEED_PWM_IN, 0);
}
void Feeder_break(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(FEED_PWM_CW, 0);       
    gpio_set_level(FEED_PWM_IN, 0);
}


static void feedmotor_boost_task(void *pvParameters)
{
    int received_data;
    
    while(1)
    {
        if (xQueueReceive(feed_motor_queue, &received_data, portMAX_DELAY) == pdPASS) {
            ESP_LOGW(TAG, "큐 수신 완료! -> [모터 구동] 속도: %d",received_data);

            while(Sliding_Back_Enable() == false)
            {
                vTaskDelay(pdMS_TO_TICKS(1000)); 
                ESP_LOGI(TAG,"SLID BACK WAIT");
            }  

            Feeder_CW();
        }
    }
}


void start_feed_motor_with_boost(int target_percentage, int Motor_motion)
{
    ESP_LOGI(TAG, "큐 전송 시도 -> 속도: %d", Motor_motion);
    BaseType_t xStatus = xQueueSend(feed_motor_queue, &Motor_motion, pdMS_TO_TICKS(100));
    
    if (xStatus == pdPASS) {
        ESP_LOGI(TAG, "큐 전송 완료!");
    } else {
        ESP_LOGE(TAG, "큐가 가득 차서 전송 실패 (Timeout)!");
    }
}

#define MOTOR_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

void init_feed_motor(void) {
// 3개 핀 비트마스크 구성 (1ULL << 핀번호)
    uint64_t pin_mask = (1ULL << FEED_PWM_IN) |
                        (1ULL << FEED_PWM_CW);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };

    // GPIO 설정 적용
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    feed_motor_queue = xQueueCreate(10, sizeof(int));
    if(feed_motor_queue == NULL)
    {
            ESP_LOGI(TAG, "feed_motor_queue fail ");
    }
  
    if (xTaskCreatePinnedToCore(
            feedmotor_boost_task,                  // 태스크 함수
            "feedmotor_boost_task",                // 태스크 이름
            MOTOR_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating feedmotor_boost_task on Core 1");
    }

    Feeder_break();
}


