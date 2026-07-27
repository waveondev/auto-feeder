#include <stdio.h>
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "app_adc.h"
#include "gpio_util.h"
#include "debug_cli.h"
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
adc_oneshot_unit_handle_t adc1_handle;



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
    #if 1
        int raw_ch0 = 0;
        int raw_ch5 = 0;
        int raw_ch6 = 0;
        int mv_ch0 = 0;
        int mv_ch5 = 0;
        int mv_ch6 = 0;
        esp_err_t err_ch;
        DBG_Resister_t* DBG_Resister = Debug_Get();
        // --- 채널 0 (GPIO 6 - ADC1) 읽기 ---
        err_ch = adc_oneshot_read(adc1_handle, ADC_CH_GPIO1, &raw_ch0);
        if (err_ch == ESP_OK) {
            if (do_cali_ch0) {
                adc_cali_raw_to_voltage(cali_ch0_handle, raw_ch0, &mv_ch0);
                if(DBG_Resister->adc)
                    ESP_LOGI(TAG, "GPIO  1 (ADC1) -> Raw: %4d | Voltage: %4d mV (%.2f V)\r\n", raw_ch0, mv_ch0, (float)mv_ch0 / 1000.0f);
            } else {
                if(DBG_Resister->adc)
                    ESP_LOGI(TAG, "GPIO  1 (ADC1) -> Raw: %4d (No Calibration)\r\n", raw_ch0);
            }
        } else {
            if(DBG_Resister->adc)
                ESP_LOGE(TAG, "Failed to read GPIO 6 (%s)", esp_err_to_name(err_ch));
        }

        err_ch = adc_oneshot_read(adc1_handle, ADC_CH_GPIO6, &raw_ch5);
        if (err_ch == ESP_OK) {
            if (do_cali_ch5) {
                adc_cali_raw_to_voltage(cali_ch5_handle, raw_ch5, &mv_ch5);
                if(DBG_Resister->adc)
                    ESP_LOGI(TAG, "GPIO  6 (ADC1) -> Raw: %4d | Voltage: %4d mV (%.2f V)\r\n", raw_ch5, mv_ch5, (float)mv_ch5 / 1000.0f);
            } else {
                if(DBG_Resister->adc)
                    ESP_LOGI(TAG, "GPIO  6 (ADC1) -> Raw: %4d (No Calibration)\r\n", raw_ch5);
            }
        } else {
            if(DBG_Resister->adc)
                ESP_LOGE(TAG, "Failed to read GPIO 6 (%s)", esp_err_to_name(err_ch));
        }

        err_ch = adc_oneshot_read(adc1_handle, ADC_CH_GPIO7, &raw_ch6);
        if (err_ch == ESP_OK) {
            if (do_cali_ch6) {
                adc_cali_raw_to_voltage(cali_ch6_handle, raw_ch6, &mv_ch6);
                if(DBG_Resister->adc)
                    ESP_LOGI(TAG, "GPIO  7 (ADC1) -> Raw: %4d | Voltage: %4d mV (%.2f V)\r\n", raw_ch6, mv_ch6, (float)mv_ch6 / 1000.0f);
            } else {
                if(DBG_Resister->adc)
                    ESP_LOGI(TAG, "GPIO  7 (ADC1) -> Raw: %4d (No Calibration)\r\n", raw_ch6);
            }
        } else {
            if(DBG_Resister->adc)
                ESP_LOGE(TAG, "Failed to read GPIO 6 (%s)", esp_err_to_name(err_ch));
        }
        #endif
}
void adc_init(void) {
#if 1
    adc_oneshot_unit_init_cfg_t init_cfg1= { .unit_id = ADC_UNIT_1 };
 
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg1, &adc1_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, 
        .atten = EXAMPLE_ADC_ATTEN,
    };
// GPIO 1 (ADC1 Channel 0) 선언
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CH_GPIO1, &chan_cfg));

    // GPIO 6 (ADC1 Channel 5) 선언
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CH_GPIO6, &chan_cfg));

    // GPIO 7 (ADC1 Channel 6) 선언
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CH_GPIO7, &chan_cfg));



    do_cali_ch0 = init_adc_calibration(CH0_ADC_UNIT, ADC_CH_GPIO1, EXAMPLE_ADC_ATTEN, &cali_ch0_handle);
    do_cali_ch5 = init_adc_calibration(CH0_ADC_UNIT, ADC_CH_GPIO6, EXAMPLE_ADC_ATTEN, &cali_ch5_handle);
    do_cali_ch6 = init_adc_calibration(CH0_ADC_UNIT, ADC_CH_GPIO7, EXAMPLE_ADC_ATTEN, &cali_ch6_handle);

    ESP_LOGI(TAG, "Dual ADC Initialized successfully ");
    #endif
}


