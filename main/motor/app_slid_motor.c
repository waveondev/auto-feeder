
#include <stdio.h>
#include "app_slid_motor.h"
#include "driver/ledc.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "gpio_util.h"
#include "esp_log.h"
#include "debug_cli.h"
#include "app_config_flash.h"
#include "tx_mqtt.h"
#include "aws_iot_task.h"
#include "driver/rmt_tx.h"
#include "app_sensor.h"

static const char *TAG = __FILE__;

rmt_channel_handle_t pwm_chan = NULL;
rmt_encoder_handle_t copy_encoder = NULL;

#define LEDC_FREQUENCY       (2000000)            // 20kHz 설정
#define LEDC_CH0_MOTOR_IN1   LEDC_CHANNEL_0


// 백그라운드 태스크 및 진행 상태 감시용 전역 변수


typedef struct {
    int target_percentage;
    Slid_Moter_e Motor_Motion; 
} slid_motor_boost_args_t;


static QueueHandle_t slid_motor_queue = NULL;

// 모터 제어 함수 (percentage: 0 ~ 100)
void set_slid_motor_speed_percent(int percentage, bool CW) {
    static rmt_symbol_word_t pwm_symbol;
    
    // 1. 안전한 범위 제한
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    uint32_t total_ticks = 1000; // 1000틱 = 50us = 20kHz
    uint32_t high_ticks = (total_ticks * percentage) / 100;
    uint32_t low_ticks = total_ticks - high_ticks;
    if(CW)
    {
        gpio_set_level(SLIDING_PWM_CW, 1);     
    }
    else
    {
        gpio_set_level(SLIDING_PWM_CW, 0);     
    }
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



void Sliding_CW(void)
{
    if(Sliding_Front_Enable())
    {
        ESP_LOGW(TAG, "FRONT MAX");        
        return;
    }

    start_slid_motor_with_boost(100,MOTOR_CW);
}
void Sliding_CCW(void)
{
    if(Sliding_Back_Enable())
    {
        ESP_LOGW(TAG, "BACK MAX");        
        return;
    }

    start_slid_motor_with_boost(100,MOTOR_CCW);
}

void Sliding_coast(void)
{
    start_slid_motor_with_boost(0,MOTOR_COAST);
}

void Sliding_break(void)
{
    start_slid_motor_with_boost(0,MOTOR_BREAK);
}

void start_slid_motor_with_boost(int target_percentage, Slid_Moter_e Motor_motion)
{
    slid_motor_boost_args_t send_data;

    if(Motor_motion == MOTOR_COAST)
    {
        set_slid_motor_speed_percent(0, true);
    }
    else if (Motor_motion == MOTOR_BREAK)
    {
        set_slid_motor_speed_percent(0, false);/* code */
    }
    
    send_data.target_percentage = target_percentage; // 10%, 20%, 30% ...
    send_data.Motor_Motion = Motor_motion;       // 2초, 4초, 6초 ...

    ESP_LOGI("SENDER", "큐 전송 시도 -> 속도: %d%%, 시간: %d초", 
                send_data.target_percentage, send_data.Motor_Motion);
    BaseType_t xStatus = xQueueSend(slid_motor_queue, &send_data, pdMS_TO_TICKS(100));
    
    if (xStatus == pdPASS) {
        ESP_LOGI("SENDER", "큐 전송 완료!");
    } else {
        ESP_LOGE("SENDER", "큐가 가득 차서 전송 실패 (Timeout)!");
    }
}


static void slidmotor_boost_task(void *pvParameters)
{
    slid_motor_boost_args_t received_data;
    bool Motor_CW = false;    
    bool Motor_enable = false;
    bool Feed_MoterEn = false;
    while(1)
    {
        if (xQueueReceive(slid_motor_queue, &received_data, pdMS_TO_TICKS(10)) == pdPASS) {
            ESP_LOGW("RECEIVER", "큐 수신 완료! -> [모터 구동] 속도: %d%%, 유지시간: %d초", 
                     received_data.target_percentage, received_data.Motor_Motion);

            switch(received_data.Motor_Motion)
            {
                case MOTOR_CW:
                    Motor_CW = false;   
                    Motor_enable = true;
                break;
                case MOTOR_CCW:
                    Motor_CW = true;    
                    Motor_enable = true;                            
                break;
                case MOTOR_COAST:
                    received_data.target_percentage = 0;
                    Motor_CW = true;    
                    Motor_enable = false;            
                break;
                case MOTOR_BREAK:
                    received_data.target_percentage = 0;
                    Motor_CW = false;
                    Motor_enable = false;            
                break;   
                case MOTOR_FEEDER:
                    Motor_CW = true;    
                    Motor_enable = true;               
                break;  
                                                             
            }
            
            set_slid_motor_speed_percent(received_data.target_percentage, Motor_CW);
        }
        if(Motor_enable == true)
        {
            if(Motor_CW == false)
            {
                if(Sliding_Front_Enable())
                {
                    Sliding_coast();
                    Motor_enable = false;
                }
            }
            else
            {
                if(Sliding_Back_Enable())
                {
                    Sliding_coast();
                    Motor_enable = false;
                    if(Feed_MoterEn)
                    {

                    }
                }
            }
        }
    }
}

#define MOTOR_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

void init_motor_ledc(void) {
// 3개 핀 비트마스크 구성 (1ULL << 핀번호)
    uint64_t pin_mask = (1ULL << SLIDING_PWM_CW) ; 


    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };

    // GPIO 설정 적용
    ESP_ERROR_CHECK(gpio_config(&io_conf));

    #if 1
    // 1. RMT TX 채널 설정 (DMA 활성화)
    rmt_tx_channel_config_t tx_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = SLIDING_PWM_IN,             // 👈 출력할 GPIO 15번 핀
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

    slid_motor_queue = xQueueCreate(10, sizeof(slid_motor_boost_args_t));
    if(slid_motor_queue == NULL)
    {
            ESP_LOGI(TAG, "slid_motor_queue fail ");
    }
  
    if (xTaskCreatePinnedToCore(
            slidmotor_boost_task,                  // 태스크 함수
            "slidmotor_boost_task",                // 태스크 이름
            MOTOR_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating slidmotor_boost_task on Core 1");
    }
    #endif
    Sliding_break();
}


