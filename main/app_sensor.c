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


#define SENSOR_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 3)
#define TASK_DELAY_MS(x) (x/portTICK_PERIOD_MS)
static const char *TAG = __FILE__;
#ifndef PACKED
#define PACKED __attribute__((packed))
#endif



void Sensor_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting sensor task");

    while (1) {
       // if(loadcell_data_get() >)
        ADC_Sensing();
       // vTaskDelay(300 / portTICK_PERIOD_MS);
        #if 1
        HX711_Sensing();
        #endif
        vTaskDelay(150 / portTICK_PERIOD_MS);
        VL53L0X_Sensing();
    }
    
}

bool sensor_init(void)
{
    static uint8_t ucParameterToPass;
    TaskHandle_t xHandle = NULL;
    bool ret = false;
    adc_init();
    ret = HX711_init();
    #if 1
    if(ret == false)
    {
        ESP_LOGE(TAG, "HX711 Error\r\n");
        return ret;
    }
    #endif
    ret = TOF_VL53L0X_init();
    #if 1
    if(ret == false)
    {
        ESP_LOGE(TAG, "TOF Error\r\n");
        return ret;
    }
    #endif
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