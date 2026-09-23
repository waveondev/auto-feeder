#include "app_TOF.h"
#include "gpio_util.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"

#include "FreeRTOS_CLI.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c.h"
#include "vl53l0x_api.h"
#include "vl53l0x_platform.h"
#include "app_config_flash.h"
#include "aws_iot_task.h"
#include "ble_tracker_id.h"
#include "debug_cli.h"
#include "app_adc.h"
static const char *TAG = __FILE__;

#if 0

#else

static void IRAM_ATTR gpio_bat_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    
    if(gpio_get_level(2) == 0)
        gpio_set_level(45, 0);
    else  
        gpio_set_level(45, 1);  
    //ESP_LOGI(TAG, "iiiio2 = %d ",gpio_get_level(2));
}



void Test_init(void)
{
    gpio_set_level(45, 1); 
    gpio_config_t io_conf = {                   
        .pin_bit_mask =(1ULL << 45),             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT_OD,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };
    gpio_config(&io_conf);


    io_conf.pin_bit_mask = (1ULL << 2);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.intr_type = GPIO_INTR_ANYEDGE;

    gpio_config(&io_conf);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(2, gpio_bat_isr_handler, (void*) 2);

}




bool VL53L0X_Detect(bool all_state)
{
    if(all_state)
    {
        if(GetTracker_Id_active())
        {
            // ESP_LOGI(TAG,"ADC = traker");
                         return true;
        }

    }
    app_config_t* app_config = get_app_config();
    if (GetIR_ADC() > app_config->tof_sense_threshold) {
       // ESP_LOGI(TAG,"ADC = %d",GetIR_ADC());
        return true;
    } else {
        return false;
    }
}


void VL53L0X_Sensing(void)
{
    gpio_set_level(IR_ENABLE, 1);   
    ADC_Sensing();
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(IR_ENABLE, 0); 
   


    //ESP_LOGI(TAG, "io2 = %d ",gpio_get_level(2));
    //vTaskDelay(500);
}

bool TOF_VL53L0X_init(void)
{    
  
    gpio_config_t io_conf = {                   
        .pin_bit_mask =(1ULL << IR_ENABLE),             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };
    gpio_config(&io_conf);
    gpio_set_level(IR_ENABLE, 0); 

    //Test_init();
    
    return true;
}
#endif
