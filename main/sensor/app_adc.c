#include <stdio.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "app_adc.h"
#include "gpio_util.h"
#include "debug_cli.h"
#include "app_slid_motor.h"
#include "esp_adc/adc_continuous.h"
#include "app_feed_motor.h"
#include "app_acc_motor.h"
#include "app_led.h"
static const char *TAG = __FILE__;
#define ADC_LEN 5

#define ADC_SAMPLE_NUM      256

static adc_channel_t adc1_dma_channels[ADC_LEN] = {ADC_CHANNEL_0, ADC_CHANNEL_1, ADC_CHANNEL_5,ADC_CHANNEL_6,ADC_CHANNEL_7};

static adc_continuous_handle_t adc_handle = NULL;

static adc_cali_handle_t cali_handle[ADC_LEN] = {0};
static bool do_cali[ADC_LEN] = {0};

static int ACC_Motor_mv = 0;
static int Bat_Adc_mv = 0;
static int SLID_Motor_mv = 0;
static int FEED_Motor_mv = 0;
static int IR_Adc_mv = 0;

int GetAcc_ADC(void)
{
    return ACC_Motor_mv;
}
int GetBat_ADC(void)
{
    return Bat_Adc_mv;
}
int GetSlid_ADC(void)
{
    return SLID_Motor_mv;
}
int GetFeed_ADC(void)
{
    return FEED_Motor_mv;
}
int GetIR_ADC(void)
{
    return IR_Adc_mv;
}

static bool init_adc_calibration(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle) {
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;
// 1. 먼저 Curve Fitting(곡선 피팅) 스키마를 지원하는지 확인하고 생성 시도
#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
            ESP_LOGI(TAG, "Curve Fitting 보정 스키마 적용 완료");
        }
        ESP_LOGI(TAG,
         "curve ret=%s (%d)",
         esp_err_to_name(ret),
         ret);
    }

#endif

    // 2. 만약 안 된다면 Line Fitting(라인 피팅) 스키마 시도
#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated) {
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK) {
            calibrated = true;
            ESP_LOGI("CALI", "Line Fitting 보정 스키마 적용 완료");
        }
    }
#endif

    *out_handle = handle;
    return calibrated;
}




void ADC_Sensing(void)
{
    if (adc_handle == NULL) {
        ESP_LOGE(TAG, "ADC handle is NULL!");
        return;
    }

    static uint8_t dma_result[ADC_SAMPLE_NUM * SOC_ADC_DIGI_DATA_BYTES_PER_CONV];
    uint32_t ret_num = 0;
    DBG_Resister_t *DBG_Resister = Debug_Get();

    static int timeout_count = 0; // 연속 타임아웃 카운트

    esp_err_t start_err = adc_continuous_start(adc_handle);
    if (start_err != ESP_OK) {
        ESP_LOGE(TAG, "ADC continuous start failed: %s", esp_err_to_name(start_err));
        return;
    }

    // 2. 아날로그 회로 안정화 및 샘플 수집을 위해 잠깐 대기 (예: 5ms)
    vTaskDelay(pdMS_TO_TICKS(10));    
    esp_err_t ret = adc_continuous_read(adc_handle, dma_result, sizeof(dma_result), &ret_num, pdMS_TO_TICKS(100));
    adc_continuous_stop(adc_handle);               

    uint32_t val_ch7 = 0, val_ch6 = 0, val_ch5 = 0, val_ch1 = 0, val_ch0 = 0;
    uint32_t cnt_ch7 = 0, cnt_ch6 = 0, cnt_ch5 = 0, cnt_ch1 = 0, cnt_ch0 = 0;

    if (ret == ESP_OK && ret_num > 0) {
        for (int i = 0; i < ret_num; i += SOC_ADC_DIGI_DATA_BYTES_PER_CONV) {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&dma_result[i];
            uint32_t chan = p->type2.channel;
            uint32_t data = p->type2.data;
            if (chan == ADC_CHANNEL_0) {
                val_ch0 += data;
                cnt_ch0++;
            } else if (chan == ADC_CHANNEL_1) {
                val_ch1 += data;
                cnt_ch1++;
            } else if (chan == ADC_CHANNEL_5) {
                val_ch5 += data;
                cnt_ch5++;
            } else if (chan == ADC_CHANNEL_6) {
                val_ch6 += data;
                cnt_ch6++;
            } else if (chan == ADC_CHANNEL_7) {
                val_ch7 += data;
                cnt_ch7++;
            } 
        }

        if (cnt_ch0) val_ch0 /= cnt_ch0;         
        if (cnt_ch1) val_ch1 /= cnt_ch1; 
        if (cnt_ch5) val_ch5 /= cnt_ch5;
        if (cnt_ch6) val_ch6 /= cnt_ch6;
        if (cnt_ch7) val_ch7 /= cnt_ch7; 


    }

    ACC_Motor_mv = val_ch0;
    Bat_Adc_mv = val_ch1;
    SLID_Motor_mv = val_ch5;
    FEED_Motor_mv = val_ch6;
    IR_Adc_mv = val_ch7;

    if (DBG_Resister && DBG_Resister->adc) {
        ESP_LOGI(TAG, "[DMA] acc_CH0: %lu | bat_CH1: %lu | slid_CH5: %lu | feed_CH6: %lu | ir_CH7: %lu", 
                 val_ch0, val_ch1, val_ch5, val_ch6, val_ch7);  
    }

    if (ret == ESP_ERR_TIMEOUT) {
        timeout_count++;
        // 연속으로 10번 이상 TIMEOUT이 발생하면 ADC 드라이버가 멈춘 것으로 판단하고 재시작
        if (timeout_count >= 10) {
            ESP_LOGW("ADC", "ADC DMA 멈춤 감지! 재시작 수행...");
        }
        if (timeout_count >= 50) {
            led_bit_enable(SENSE_ERR_BIT);
            ESP_LOGW("ADC", "ADC ERROR");
        }

        return; 
    } else if (ret != ESP_OK) {
        ESP_LOGE("ADC", "ADC Read Error: %s", esp_err_to_name(ret));
        return;
    }
}
void adc_init(void) {
// 1. DMA 핸들 생성
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 1024,
        .conv_frame_size = ADC_SAMPLE_NUM * SOC_ADC_DIGI_DATA_BYTES_PER_CONV,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    // 2. 3개 채널(GPIO 1, 6, 7) 패턴 등록
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 20 * 1000,           // 20kHz
        .conv_mode = ADC_CONV_SINGLE_UNIT_1, // ADC1 단독 사용
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    adc_digi_pattern_config_t adc_pattern[ADC_LEN] = {0};
    dig_cfg.pattern_num = ADC_LEN;

    for (int i = 0; i < ADC_LEN; i++) {
        adc_pattern[i].atten = ADC_ATTEN_DB_12;
        adc_pattern[i].channel = adc1_dma_channels[i] & 0x7;
        adc_pattern[i].unit = ADC_UNIT_1;
        adc_pattern[i].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
    }

    dig_cfg.adc_pattern = adc_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    // 3. 캘리브레이션은 기존 방식 그대로 유지!
    #if 0
    for (int i = 0; i < ADC_LEN; i++) {
        do_cali[i] = init_adc_calibration(CH0_ADC_UNIT, adc1_dma_channels[i], EXAMPLE_ADC_ATTEN, &cali_handle[i]);
    }
    #endif
    // 4. DMA 수집 시작
    //ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    ESP_LOGI(TAG, "ADC DMA Mode (3 Channels) Initialized successfully");
}


