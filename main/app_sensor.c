#include "app_sensor.h"
#include "esp_system.h"
#include "esp_err.h"

#include "FreeRTOS_CLI.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
// 구버전 ESP-IDF용 I2C 헤더 경로

#include "app_HX711.h"
#include "app_TOF.h"
#include "app_adc.h"
#include "app_led.h"
#include "gpio_util.h"
#define SENSOR_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define TASK_DELAY_MS(x) (x/portTICK_PERIOD_MS)
static const char *TAG = __FILE__;
#ifndef PACKED
#define PACKED __attribute__((packed))
#endif



void Sensor_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting sensor task");
    bool ret = true; 
    int error_count = 0 ;
    adc_init();

    ret = TOF_VL53L0X_init();
    #if 1
    if(ret == false)
    {
        ESP_LOGE(TAG, "TOF Error\r\n");
       // return ret;
       led_bit_enable(SENSE_ERR_BIT);
    }
    #endif

// 3개 핀 비트마스크 구성 (1ULL << 핀번호)
    uint64_t pin_mask = (1ULL << SLIDING_A_SEN) | 
                        (1ULL << SLIDING_B_SEN) | 
                        (1ULL << FEED_SEN)| 
                        (1ULL << IR_OUT0);

    gpio_config_t io_conf = {                   
        .pin_bit_mask = pin_mask,             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_INPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };
    gpio_config(&io_conf);



    while (1) {
        ADC_Sensing();
        #if 1
        #endif
        VL53L0X_Sensing();
        //ESP_LOGI(TAG, "gpio_set_level(IR) = %d\r\n",gpio_get_level(IR_OUT0));
        vTaskDelay(30 / portTICK_PERIOD_MS);
    }
    
}

bool sensor_init(void)
{
    static uint8_t ucParameterToPass;
    TaskHandle_t xHandle = NULL;
        gpio_config_t io_conf = {                   
        .pin_bit_mask = (1ULL << IR_ONOFF0),             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };
    gpio_config(&io_conf);

    gpio_set_level(IR_ONOFF0, 1);   

    HX711_task_init();
    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            Sensor_task,                  // 태스크 함수
            "sensor_task",                // 태스크 이름
            SENSOR_TASK_STACK_SIZE,       // 스택 크기
            &ucParameterToPass,        // 파라미터
            tskIDLE_PRIORITY + 4,      // 우선순위
            &xHandle,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        ESP_LOGE(TAG, "Error creating Sensor_task on Core 1");
    }

    return true;
}