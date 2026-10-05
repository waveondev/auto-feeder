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


#include <stdio.h>
#include <string.h>
#include "esp_adc/adc_oneshot.h"
#include "app_adc.h"
#include "driver/ledc.h"
static const char *TAG = __FILE__;

static int ir_count = 0;
bool detect_object(void);
bool VL53L0X_Detect(bool all_state)
{
    if(all_state)
    {
        if(GetTracker_Id_active())
        {
            return true;
        }
    }
    app_config_t* app_config = get_app_config();
    #if 1
    if (ir_count >= 5) {
        return true;
        
    } else {
        return false;
    }
    #endif
}
int left_value = 0;
int right_value = 0;
void detect_task(void)
{
    app_config_t* app_config = get_app_config();
    if (right_value > app_config->tof_sense_threshold) {
        if(ir_count < 10)
            ir_count++;
        
    } else {
        if(ir_count)
            ir_count=0;
    }
}


void VL53L0X_Sensing(void)
{
    ADC_Sensing();
    detect_object();
    detect_task();
}


// ================= 설 정 값 =================
// TX (발광부) 설정

#define LEDC_TIMER          LEDC_TIMER_0
#define LEDC_MODE           LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL        LEDC_CHANNEL_1
#define LEDC_DUTY           127             // 50% Duty Cycle (8비트 기준 127)
#define LEDC_FREQUENCY      38000           // 38kHz

// RX (수광부) 설정
#define IR_RX_ADC_UNIT      ADC_UNIT_1      
#define IR_RX_ADC_CHANNEL   ADC_CHANNEL_6   // GPIO34에 해당 (보드에 맞게 변경)
#define DETECT_THRESHOLD    300             // 물체 감지 임계값 (환경에 맞게 튜닝 필요)

// ADC 핸들 전역 변수
adc_oneshot_unit_handle_t adc_handle;

// ================= 함수 구현 =================

// 1. 발광부(TX) PWM 초기화
bool TOF_VL53L0X_init(void) {
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_TIMER_8_BIT,
        .freq_hz          = LEDC_FREQUENCY, 
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);

    ledc_channel_config_t ledc_channel = {
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .timer_sel      = LEDC_TIMER,
        .intr_type      = LEDC_INTR_DISABLE,
        .gpio_num       = IR_ENABLE,
        .duty           = 0, // 처음엔 꺼둠
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
    return true;
}

void ir_tx_enable(bool enable) {
    if (enable) {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, LEDC_DUTY);
    } else {
        ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, 0);
    }
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// 4. 핵심 로직: 물체 감지 (배경광 빼기)
bool detect_object(void) {
    int val_right_off = 0;
    int val_right_on = 0;

    // 단계 1: TX OFF 상태에서 주변 태양광(노이즈) 측정
    ir_tx_enable(false);
    IR_SenSing();
    


    val_right_off = GetIR_ADC();
    // 단계 2: TX ON 상태에서 (태양광 + 반사된 IR 빛) 측정
    ir_tx_enable(true);
    vTaskDelay(1);
    IR_SenSing();

    val_right_on = GetIR_ADC();

    // 단계 3: TX 다시 OFF
    ir_tx_enable(false);


    right_value = (val_right_on - val_right_off - (val_right_off/50)*4);
    if(right_value < 0)
    right_value = 0;

    return false;
}   

