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
#define CH0_ADC_CHANNEL         ADC_CHANNEL_5  // GPIO 6
#define EXAMPLE_ADC_ATTEN       ADC_ATTEN_DB_12 



static adc_cali_handle_t cali_ch0_handle = NULL;
static bool do_cali_ch0 = false;
adc_oneshot_unit_handle_t adc0_handle;



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
        int raw_ch0 = 0;
        int mv_ch0 = 0;
        esp_err_t err_ch0;

        // --- 채널 0 (GPIO 6 - ADC1) 읽기 ---
        err_ch0 = adc_oneshot_read(adc0_handle, CH0_ADC_CHANNEL, &raw_ch0);
        #if 1
        if (err_ch0 == ESP_OK) {
            if (do_cali_ch0) {
                adc_cali_raw_to_voltage(cali_ch0_handle, raw_ch0, &mv_ch0);
                //ESP_LOGI(TAG, "GPIO  6 (ADC1) -> Raw: %4d | Voltage: %4d mV (%.2f V)\r\n", raw_ch0, mv_ch0, (float)mv_ch0 / 1000.0f);
            } else {
                //ESP_LOGI(TAG, "GPIO  6 (ADC1) -> Raw: %4d (No Calibration)\r\n", raw_ch0);
            }
        } else {
            ESP_LOGE(TAG, "Failed to read GPIO 6 (%s)", esp_err_to_name(err_ch0));
        }
            #endif
}
void adc_init(void) {

    adc_oneshot_unit_init_cfg_t init_cfg0 = { .unit_id = ADC_UNIT_1 };
 
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg0, &adc0_handle));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .bitwidth = ADC_BITWIDTH_DEFAULT, 
        .atten = EXAMPLE_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc0_handle, CH0_ADC_CHANNEL, &chan_cfg));

    do_cali_ch0 = init_adc_calibration(CH0_ADC_UNIT, CH0_ADC_CHANNEL, EXAMPLE_ADC_ATTEN, &cali_ch0_handle);


    ESP_LOGI(TAG, "Dual ADC Initialized successfully ");
}


