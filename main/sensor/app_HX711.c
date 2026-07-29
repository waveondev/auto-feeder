#include "app_HX711.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "gpio_util.h"
#include "FreeRTOS_CLI.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "app_config_flash.h"
#include "app_led.h"
#include "debug_cli.h"
#include "tx_mqtt.h"
#include "aws_iot_task.h"
static const char *TAG = __FILE__;

static float filtered_weight = 0.0f; // 현재 필터링된 최종 무게값
#include "math.h"
#if 0 
[SENSOR] Weight: 239.6 g (raw: 251934) 없을때
[SENSOR] Weight: 302.02 g (raw: 314222) 물통만 
[SENSOR] Weight: 347.7 g (raw: 346721) 모터+물통
[SENSOR] Weight: 379.8 g (raw: 366308) 전체

I (795771) ./main/app_config_flash.c:   - HX1 Scale Factor     : 126.67
I (795771) ./main/app_config_flash.c:   - HX1 Tare Offset      : 385928
I (795781) ./main/app_config_flash.c:   - case_raw_data        : 386878

I (1743541) ./main/app_config_flash.c:   - HX1 Scale Factor     : 182.23
I (1743541) ./main/app_config_flash.c:   - HX1 Tare Offset      : 260188
I (1743551) ./main/app_config_flash.c:   - case_raw_data        : 261253

#endif

#include "hx711_lib.h"
static int32_t hx711_data;
static int32_t hx711_data_buf;

hx711_t dev = {
    .dout = PIN_HX711_DOUT,
    .pd_sck = PIN_HX711_SCK,
    .gain = HX711_GAIN_A_64
};
static uint16_t hx711_cal_enable = 0;

static void HX711_cal_process(void)
{
    app_config_t* app_config = get_app_config();

    int32_t cal_data = 0;

    while(hx711_read_average(&dev, 100, &cal_data) != ESP_OK){}
    app_config->case_raw_data = cal_data;
    app_nvs_save_set();
    ESP_LOGI(TAG, "Tare offset set to %d\r\n", app_config->hx1_offset);
}
void HX711_cal_init(uint16_t cal)
{
    hx711_cal_enable = 1;
}
float loadcell_data_get(void)
{
    int case_weight = 0;
    app_config_t* app_config = get_app_config();
    case_weight = app_config->case_raw_data;
    int32_t case_data = hx711_data_buf - case_weight;
    float Data = (float)case_data / 100.0f;
    return Data;
}
void HX711_Sensing(void)
{
    esp_err_t r;
    DBG_Resister_t *DBG_Resister = Debug_Get();
    app_config_t* app_config = get_app_config();
    if(hx711_cal_enable)
    {
        hx711_cal_enable = 0;
        HX711_cal_process();
    }    
    r = hx711_read_average(&dev, 5, &hx711_data);
    if (r != ESP_OK)
    {
        ESP_LOGE(TAG, "Could not read data: %d (%s)", r, esp_err_to_name(r));
        return;
    }
    hx711_data_buf = hx711_data;
    if(DBG_Resister->HX711)
    {
            ESP_LOGI(TAG, "hx711_data: (%d)",hx711_data_buf);
    }



    if(DBG_Resister->HX711)
    {
        ESP_LOGI(TAG, " Raw: %.2f g", loadcell_data_get());
    }

}
#define HX711_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)



static void HX711_task(void *pvParameter)
{
    ESP_LOGI(TAG, "Starting sensor task");
    
    //ESP_LOGE(TAG, "HX711 Error %d \r\n", hx711_init(&dev));
    #if 1
    if(hx711_init(&dev) != ESP_OK)
    {
        ESP_LOGE(TAG, "HX711 Error\r\n");
        led_bit_enable(SENSE_ERR_BIT);
    }
    #endif

    while (1) {

        HX711_Sensing();
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    
}


bool HX711_task_init(void)
{

    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            HX711_task,                  // 태스크 함수
            "HX711_task",                // 태스크 이름
            HX711_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        ESP_LOGE(TAG, "Error creating Sensor_task on Core 1");
    }

    return true;
}




