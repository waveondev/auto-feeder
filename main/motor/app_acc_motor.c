
#include <stdio.h>
#include "app_acc_motor.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_util.h"
#include "esp_log.h"
#include "app_led.h"
#include "debug_cli.h"
#include "app_config_flash.h"
#include "tx_mqtt.h"
#include "aws_iot_task.h"
#include "app_sensor.h"

static const char *TAG = __FILE__;
static QueueHandle_t acc_motor_queue = NULL;
static bool is_motor_running = false; // 모터 동작 중 플래그

void Accum_Set(bool status)
{
    if(status)
        gpio_set_level(ACUUM_PWM_IN, 1);       
    else
        gpio_set_level(ACUUM_PWM_IN, 0);   
}

void start_acc_motor_with_boost(int target_percentage, int Motor_motion)
{
    // 이미 모터가 동작 중이면 중복 요청 방지
    if (is_motor_running) {
        ESP_LOGW(TAG, "모터가 이미 동작 중입니다.");
        return;
    }

    ESP_LOGI(TAG, "큐 전송 시도 -> 속도: %d", Motor_motion);
    BaseType_t xStatus = xQueueSend(acc_motor_queue, &Motor_motion, pdMS_TO_TICKS(100));
    
    if (xStatus != pdPASS) {
        ESP_LOGE(TAG, "큐가 가득 차서 전송 실패!");
    }
}

// 진공 모터 전용 제어 태스크
static void acc_motor_boost_task(void *pvParameters)
{
    int received_data;
    
    while(1)
    {
        // 큐에서 명령이 들어올 때까지 대기
        if (xQueueReceive(acc_motor_queue, &received_data, portMAX_DELAY) == pdPASS) {
            ESP_LOGW(TAG, "큐 수신 완료! -> [진공 모터 구동] 속도: %d", received_data);
            
            is_motor_running = true;
            uint8_t retry_count = 0;
            bool vacuum_success = false;

   
            Accum_Set(true);

            while (retry_count < 3 && !vacuum_success) {
                
                for (int i = 0; i < 600; i++) {
                    vTaskDelay(pdMS_TO_TICKS(100));


                    // int adc_val = get_adc();
                    int adc_val = 150; 

                    if (adc_val < 200) {
                        ESP_LOGI(TAG, "진공 도달 성공! (ADC: %d)", adc_val);
                        vacuum_success = true;
                        break; 
                    }
                }

                if (!vacuum_success) {
                    retry_count++;
                    ESP_LOGW(TAG, "진공 도달 실패.. 재시도 횟수: %d/3", retry_count);
                }
            }

            Accum_Set(false);
            if (!vacuum_success) {
                ESP_LOGE(TAG, "최종 진공 형성 실패 (3회 타임아웃)");
            }
            
            is_motor_running = false;
        }
    }
}


#define MOTOR_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

void init_acc_motor(void) {
// 3개 핀 비트마스크 구성 (1ULL << 핀번호)
    uint64_t pin_mask = (1ULL << ACUUM_PWM_IN);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };

    // GPIO 설정 적용
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    acc_motor_queue = xQueueCreate(10, sizeof(int));
    if(acc_motor_queue == NULL)
    {
            ESP_LOGI(TAG, "acc_motor_queue fail ");
    }
    
    if (xTaskCreatePinnedToCore(
            acc_motor_boost_task,                  // 태스크 함수
            "acc_motor_boost_task",                // 태스크 이름
            MOTOR_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating acc_motor_boost_task on Core 1");
    }
    Accum_Set(false);
}


