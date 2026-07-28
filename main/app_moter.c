
#include <stdio.h>
#include "app_moter.h"
#include "driver/ledc.h"
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
#include "driver/rmt_tx.h"


static const char *TAG = __FILE__;


rmt_channel_handle_t pwm_chan = NULL;
rmt_encoder_handle_t copy_encoder = NULL;

#define MOTOR_IN1_GPIO       (SLIDING_PWM_IN)
#define LEDC_FREQUENCY       (2000000)            // 20kHz 설정

#define LEDC_CH0_MOTOR_IN1   LEDC_CHANNEL_0

static int current_target_percentage = 0; 

// 모터 제어 함수 (percentage: 0 ~ 100)
void set_motor_speed_percent(int percentage) {
    static rmt_symbol_word_t pwm_symbol;
    
    // 1. 안전한 범위 제한
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    uint32_t total_ticks = 1000; // 1000틱 = 50us = 20kHz
    uint32_t high_ticks = (total_ticks * percentage) / 100;
    uint32_t low_ticks = total_ticks - high_ticks;

    // 2. v5.x 방식: 구조체에 직접 레벨(Level)과 지속시간(Duration) 대입
    if (percentage == 100) {
        // 100% 듀티: 계속 High (0 방지용으로 반반 쪼개기)
        pwm_symbol.level0 = 1;
        pwm_symbol.duration0 = total_ticks / 2;
        pwm_symbol.level1 = 1;
        pwm_symbol.duration1 = total_ticks - (total_ticks / 2);
    } else if (percentage == 0) {
        // 0% 듀티: 계속 Low
        pwm_symbol.level0 = 0;
        pwm_symbol.duration0 = total_ticks / 2;
        pwm_symbol.level1 = 0;
        pwm_symbol.duration1 = total_ticks - (total_ticks / 2);
    } else {
        // 일반 PWM: High 이후 Low
        pwm_symbol.level0 = 1;
        pwm_symbol.duration0 = high_ticks;
        pwm_symbol.level1 = 0;
        pwm_symbol.duration1 = low_ticks;
    }

    // 3. 무한 루프 송출 설정 (-1)
    rmt_transmit_config_t tx_config = {
        .loop_count = -1, 
    };

    rmt_disable(pwm_chan); 
    rmt_enable(pwm_chan);

    // 4. DMA를 통해 심볼 전송 시작 (기존 송출을 덮어씀)
    ESP_ERROR_CHECK(rmt_transmit(pwm_chan, copy_encoder, &pwm_symbol, sizeof(pwm_symbol), &tx_config));

}


// 백그라운드 태스크 및 진행 상태 감시용 전역 변수
static TaskHandle_t xMotorBoostTaskHandle = NULL;
static int duration_sec_buf = 0; // 0이 아니면 현재 "시간 제한 모드"가 작동 중임을 의미

typedef struct {
    int target_percentage;
    int duration_sec; 
} motor_boost_args_t;

motor_boost_args_t boost_config;
static QueueHandle_t motor_queue = NULL;

void Sliding_CW(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(SLIDING_PWM_CW, 0);       
    gpio_set_level(SLIDING_PWM_IN, 1);
}
void Sliding_CCW(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(SLIDING_PWM_CW, 1);       
    gpio_set_level(SLIDING_PWM_IN, 1);
}
void Sliding_OFF(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
   // gpio_set_level(SLIDING_PWM_CW, 0);       
   // gpio_set_level(SLIDING_PWM_IN, 0);
   start_motor_with_boost(0,0);
}


void Feeder_CW(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(FEED_PWM_IN, 0);       
    gpio_set_level(FEED_PWM_IN, 1);
}
void Feeder_CCW(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(FEED_PWM_IN, 1);       
    gpio_set_level(FEED_PWM_IN, 1);
}
void Feeder_OFF(void)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    gpio_set_level(FEED_PWM_IN, 0);       
    gpio_set_level(FEED_PWM_IN, 0);
}

void Accum_Set(bool status)
{
       // 기본 초기 출력 상태를 LOW(0)로 세팅
    if(status)
        gpio_set_level(ACUUM_PWM_IN, 1);       
    else
        gpio_set_level(ACUUM_PWM_IN, 0);   
}



void motor_all_off(void)
{
    Sliding_OFF();
    Feeder_OFF();
    Accum_Set(false);
}


static void motor_boost_task(void *pvParameters)
{
    motor_boost_args_t received_data;
    while(1)
    {
        if (xQueueReceive(motor_queue, &received_data, portMAX_DELAY) == pdPASS) {
            ESP_LOGW("RECEIVER", "큐 수신 완료! -> [모터 구동] 속도: %d%%, 유지시간: %d초", 
                     received_data.target_percentage, received_data.duration_sec);
            
            duration_sec_buf = received_data.duration_sec;
            current_target_percentage = received_data.target_percentage;
            // 💡 이곳에 이전에 만든 RMT 모터 제어 함수를 넣으면 됩니다!
           // for(int i=0;i<100;i++)
            {
           //     set_motor_speed_percent(i);
            //    vTaskDelay(pdMS_TO_TICKS(20)); // 정확히 1초(1000ms)만 대기
            }
            if(duration_sec_buf)
            {
                gpio_set_level(SLIDING_PWM_CW, 1);     
            }
            else
            {
                gpio_set_level(SLIDING_PWM_CW, 0);     
            }
            set_motor_speed_percent(received_data.target_percentage);
            vTaskDelay(pdMS_TO_TICKS(3000));            
         }
    }
}


void start_motor_with_boost(int target_percentage, bool Motor_CW_CCW)
{
    motor_boost_args_t send_data;

    if (target_percentage <= 0) {
       // ESP_LOGI(TAG, "[PUMP] 🛑 모터 즉시 정지 (0%%)");
        set_motor_speed_percent(0);
        current_target_percentage = 0;
        return; 
    }

    if (target_percentage == current_target_percentage) {
        return; 
    }

    send_data.target_percentage = target_percentage; // 10%, 20%, 30% ...
    send_data.duration_sec = Motor_CW_CCW?1:0;       // 2초, 4초, 6초 ...

    ESP_LOGI("SENDER", "큐 전송 시도 -> 속도: %d%%, 시간: %d초", 
                send_data.target_percentage, send_data.duration_sec);
    BaseType_t xStatus = xQueueSend(motor_queue, &send_data, pdMS_TO_TICKS(100));
    
    if (xStatus == pdPASS) {
        ESP_LOGI("SENDER", "큐 전송 완료!");
    } else {
        ESP_LOGE("SENDER", "큐가 가득 차서 전송 실패 (Timeout)!");
    }
}
#define MOTOR_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

void init_motor_ledc(void) {
// 3개 핀 비트마스크 구성 (1ULL << 핀번호)
    uint64_t pin_mask = (1ULL << FEED_PWM_IN)    | 
                        (1ULL << SLIDING_PWM_CW)    | 
                        (1ULL << FEED_PWM_CW)    | 
                        (1ULL << ACUUM_PWM_IN);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };

    // GPIO 설정 적용
    ESP_ERROR_CHECK(gpio_config(&io_conf));

   
    ESP_LOGI(TAG, "GPIO 15, 16, 2 Output Initialized Successfully!");

    #if 1
    // 1. RMT TX 채널 설정 (DMA 활성화)
    rmt_tx_channel_config_t tx_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = MOTOR_IN1_GPIO,             // 👈 출력할 GPIO 15번 핀
        .mem_block_symbols = 128,    // DMA 내부 블록 사이즈
        .resolution_hz = LEDC_FREQUENCY,  // 🌟 20MHz 해상도 (1틱 = 50ns)
        .trans_queue_depth = 4,
        .flags.with_dma = true,     // 🌟 핵심: DMA 통신 켜기
    };
    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &pwm_chan));

    // 2. 심볼(Symbol) 데이터를 그대로 복사해주는 기본 인코더 사용
    rmt_copy_encoder_config_t encoder_config = {};
    ESP_ERROR_CHECK(rmt_new_copy_encoder(&encoder_config, &copy_encoder));

    // 3. RMT 채널 하드웨어 켜기
    ESP_ERROR_CHECK(rmt_enable(pwm_chan));

    motor_queue = xQueueCreate(10, sizeof(motor_boost_args_t));
    if(motor_queue == NULL)
    {
            ESP_LOGI(TAG, "motor_queue fail ");
    }
    motor_all_off();
    if (xTaskCreatePinnedToCore(
            motor_boost_task,                  // 태스크 함수
            "motor_boost_task",                // 태스크 이름
            MOTOR_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating motor_boost_task on Core 1");
    }
    #endif
}


