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
static const char *TAG = __FILE__;


#define CH0_ADC_UNIT            ADC_UNIT_1
// ESP32-S3 GPIO -> ADC1 채널 매핑
#define ADC_CH_GPIO1   ADC_CHANNEL_0  // GPIO 1
#define ADC_CH_GPIO6   ADC_CHANNEL_5  // GPIO 6
#define ADC_CH_GPIO7   ADC_CHANNEL_6  // GPIO 7
#define EXAMPLE_ADC_ATTEN       ADC_ATTEN_DB_12 



static adc_cali_handle_t cali_ch0_handle = NULL;
static adc_cali_handle_t cali_ch5_handle = NULL;
static adc_cali_handle_t cali_ch6_handle = NULL;
static bool do_cali_ch0 = false;
static bool do_cali_ch5 = false;
static bool do_cali_ch6 = false;
static int ch0_mv = 0;
static int ch5_mv = 0;
static int ch6_mv = 0;  
adc_continuous_handle_t adc_handle = NULL;


int GetAcc_ADC(void)
{
    return ch0_mv;
}
int GetSlid_ADC(void)
{
    return ch5_mv;
}
int GetFeed_ADC(void)
{
    return ch6_mv;
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


#if 0
mv_ch0 > 800
#endif

void ADC_Sensing(void)
{
    uint8_t result_buf[36] = {0};
    uint32_t ret_num = 0;
    uint32_t number_sum = 0;
    esp_err_t ret =ESP_OK;
  
    // DMA 버퍼 읽기
    // 10ms 동안 수집된 DMA 데이터 읽기
    ret = adc_continuous_read(adc_handle,
                            result_buf,
                            sizeof(result_buf),
                            &ret_num,
                            0);

    DBG_Resister_t* DBG_Resister = Debug_Get();
    if (ret == ESP_OK)
    {
        uint32_t chan = 0;
        uint32_t raw  = 0;

        // SOC_ADC_DIGI_DATA_BYTES(4바이트 또는 2바이트) 보폭으로 파싱
        for (int i = 0; i < ret_num; i += CONFIG_SOC_ADC_DIGI_DATA_BYTES_PER_CONV)
        {
            adc_digi_output_data_t *p = (adc_digi_output_data_t *)&result_buf[i];
            
            // ESP-IDF v5.x 공통 구조체 접근
            chan = p->type2.channel;
            raw  = p->type2.data;
            int mv_out = 0;

            switch (chan)
            {
                case ADC_CHANNEL_0: // GPIO 1
                    if (do_cali_ch0) {
                        adc_cali_raw_to_voltage(cali_ch0_handle, raw, &mv_out);
                        ch0_mv = mv_out;
                        if (mv_out > 300) {
                            Accum_Set(false);
                        }
                    }
                    
                break;

                case ADC_CHANNEL_5: // GPIO 6
                    if (do_cali_ch5) {
                        adc_cali_raw_to_voltage(cali_ch5_handle, raw, &mv_out);
                        ch5_mv = mv_out;
                    }
                break;

                case ADC_CHANNEL_6: // GPIO 7
                    if (do_cali_ch6) {
                        adc_cali_raw_to_voltage(cali_ch6_handle, raw, &mv_out);
                        ch6_mv = mv_out;
                        if(mv_out > 600)
                            Feeder_break();   
                    }
                break;

                default:
                    break;
            }
        }
        switch (chan)
        {
            case ADC_CHANNEL_0: // GPIO 1
                if (do_cali_ch0) {
                    if(DBG_Resister->adc)
                        ESP_LOGI(TAG, "GPIO  1 (ADC1) Voltage: %4d mV (%.2f V)\r\n", ch0_mv, (float)ch0_mv / 1000.0f);
                }
                
                break;

            case ADC_CHANNEL_5: // GPIO 6
                if (do_cali_ch5) {
                    if(DBG_Resister->adc)
                    ESP_LOGI(TAG, "GPIO  6 (ADC1)  Voltage: %4d mV (%.2f V)\r\n", ch5_mv, (float)ch5_mv / 1000.0f);
                }
                break;

            case ADC_CHANNEL_6: // GPIO 7
                if (do_cali_ch6) {
                    if(DBG_Resister->adc)
                        ESP_LOGI(TAG, "GPIO  7 (ADC1) Voltage: %4d mV (%.2f V)\r\n", ch6_mv, (float)ch6_mv / 1000.0f);                        
                }
                break;

            default:
                break;
        } 
    }
}
void adc_init(void) {
// 1. DMA 핸들 생성
    adc_continuous_handle_cfg_t handle_cfg = {
        .max_store_buf_size = 256,
        .conv_frame_size = 36,
    };
    ESP_ERROR_CHECK(adc_continuous_new_handle(&handle_cfg, &adc_handle));

    // 2. 3개 채널(GPIO 1, 6, 7) 패턴 등록
    adc_continuous_config_t dig_cfg = {
        .sample_freq_hz = 2000, // 20kHz 샘플링
        .conv_mode = ADC_CONV_SINGLE_UNIT_1, // ADC1 단독 사용
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };

    adc_digi_pattern_config_t adc_pattern[3] = {0};

    // [채널 0] GPIO 1 (ADC_CHANNEL_0)
    adc_pattern[0].atten = EXAMPLE_ADC_ATTEN;
    adc_pattern[0].channel = ADC_CHANNEL_0;
    adc_pattern[0].unit = ADC_UNIT_1;
    adc_pattern[0].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    // [채널 5] GPIO 6 (ADC_CHANNEL_5)
    adc_pattern[1].atten = EXAMPLE_ADC_ATTEN;
    adc_pattern[1].channel = ADC_CHANNEL_5;
    adc_pattern[1].unit = ADC_UNIT_1;
    adc_pattern[1].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    // [채널 6] GPIO 7 (ADC_CHANNEL_6)
    adc_pattern[2].atten = EXAMPLE_ADC_ATTEN;
    adc_pattern[2].channel = ADC_CHANNEL_6;
    adc_pattern[2].unit = ADC_UNIT_1;
    adc_pattern[2].bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;

    dig_cfg.pattern_num = 3;
    dig_cfg.adc_pattern = adc_pattern;

    ESP_ERROR_CHECK(adc_continuous_config(adc_handle, &dig_cfg));

    // 3. 캘리브레이션은 기존 방식 그대로 유지!
    do_cali_ch0 = init_adc_calibration(CH0_ADC_UNIT, ADC_CHANNEL_0, EXAMPLE_ADC_ATTEN, &cali_ch0_handle);
    do_cali_ch5 = init_adc_calibration(CH0_ADC_UNIT, ADC_CHANNEL_5, EXAMPLE_ADC_ATTEN, &cali_ch5_handle);
    do_cali_ch6 = init_adc_calibration(CH0_ADC_UNIT, ADC_CHANNEL_6, EXAMPLE_ADC_ATTEN, &cali_ch6_handle);

    // 4. DMA 수집 시작
    ESP_ERROR_CHECK(adc_continuous_start(adc_handle));

    ESP_LOGI(TAG, "ADC DMA Mode (3 Channels) Initialized successfully");
}


