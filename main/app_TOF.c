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
uint32_t _trace_level;
int _modules;
static const char *TAG = __FILE__;

// ⭐️ ST 공식 API용 디바이스 구조체 전역 변수 선언 (두 채널 분리)
static VL53L0X_Dev_t dev_tof0;


static bool g_tof0_ok = false;
static uint32_t g_tof0_last_ok_ms = 0;


static uint16_t tof0_mm;

uint32_t ts_tof0_ms = 0;


#define FILTER_SIZE 10
static uint32_t TOF_Buf_L[FILTER_SIZE] = {0};
static uint32_t TOF_Buf_R[FILTER_SIZE] = {0};
static int buffer_idx_L = 0,    buffer_idx_R = 0;

/**
 * @brief 새로운 데이터를 필터 버퍼에 추가하는 함수
 * @param new_value 새로 측정된 센서 값
 */
static void moving_average_update_l(uint16_t new_value) {
    TOF_Buf_L[buffer_idx_L] = new_value;
    buffer_idx_L = (buffer_idx_L + 1) % FILTER_SIZE;
}
static void moving_average_update_r(uint16_t new_value) {
    TOF_Buf_R[buffer_idx_R] = new_value;
    buffer_idx_R = (buffer_idx_R + 1) % FILTER_SIZE;
}

/**
 * @brief 현재 버퍼에 쌓인 데이터들의 평균값을 계산하여 반환하는 함수
 * @return float 최근 FILTER_SIZE 개수의 평균값
 */
uint16_t moving_average_get_L(void) {
    uint32_t sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += TOF_Buf_L[i];
    }

    return (uint16_t)(sum / FILTER_SIZE);
}
uint16_t moving_average_get_R(void) {
    uint32_t sum = 0;
    for (int i = 0; i < FILTER_SIZE; i++) {
        sum += TOF_Buf_R[i];
    }

    return (uint16_t)(sum / FILTER_SIZE);
}
// 💡 내부 헬퍼 함수: 단일 센서 ST C API 초기화 및 아크릴 보정 
static bool init_single_vl53l0x(VL53L0X_Dev_t *pDevice, i2c_port_t i2c_port, const char *sensor_name)
{
    VL53L0X_Error status;
    uint32_t refSpadCount;
    uint8_t isApertureSpads;
    uint8_t VhvSettings;
    uint8_t PhaseCal;

    // 1. 플랫폼 바인딩 세팅 (I2C 포트와 기본 주소 0x29)
    pDevice->i2c_port_num = i2c_port;
    pDevice->i2c_address  = 0x29; 

    // 2. ST C API 기본 데이터 및 스태틱 초기화
    status = VL53L0X_DataInit(pDevice);
    if (status != VL53L0X_ERROR_NONE) {
        ESP_LOGE(TAG, "[%s] DataInit Failed: %d", sensor_name, status);
        return false;
    }
    status = VL53L0X_StaticInit(pDevice);
    if (status != VL53L0X_ERROR_NONE) {
        ESP_LOGE(TAG, "[%s] StaticInit Failed: %d", sensor_name, status);
        return false;
    }

    // 3. SPAD 관리 및 내부 기준치 온도시정(Calibration)
    status = VL53L0X_PerformRefSpadManagement(pDevice, &refSpadCount, &isApertureSpads);
    if (status != VL53L0X_ERROR_NONE) return false;

    status = VL53L0X_PerformRefCalibration(pDevice, &VhvSettings, &PhaseCal);
    if (status != VL53L0X_ERROR_NONE) return false;

    // 4. 싱글 측정 모드 및 타임예산(33ms) 세팅
    status = VL53L0X_SetDeviceMode(pDevice, VL53L0X_DEVICEMODE_SINGLE_RANGING);
    if (status != VL53L0X_ERROR_NONE) return false;

    status = VL53L0X_SetMeasurementTimingBudgetMicroSeconds(pDevice, 33000);
    if (status != VL53L0X_ERROR_NONE) return false;

    // 5. ⭐️ [핵심] 아크릴 크로스토크(Xtalk) 자동 학습 보정 ⭐️
    // ⚠️ 주의: 기기에 아크릴을 조립한 상태에서 센서 정면 정확히 10cm(100mm) 지점에 하얀 벽/종이를 대고 전원을 켜야 합니다!
    FixPoint1616_t target_distance_mm = 100;
    uint32_t xtalk_rate_mcps = 0;

    ESP_LOGI(TAG, "[%s] 아크릴 반사광(Xtalk) 보정 연산 시작... (10cm 앞에 타겟 유지 필요)", sensor_name);
    status = VL53L0X_PerformXTalkCalibration(pDevice, target_distance_mm, &xtalk_rate_mcps);
    
    if (status == VL53L0X_ERROR_NONE) {
        ESP_LOGI(TAG, "[%s] 아크릴 보정 완료! 노이즈 세기: %d Mcps", sensor_name, (int)xtalk_rate_mcps);
        VL53L0X_SetXTalkCompensationEnable(pDevice, 1); // 보정 엔진 활성화
    } else {
        ESP_LOGE(TAG, "[%s] 아크릴 보정 실패 (%d). 보정 없이 측정을 진행합니다.", sensor_name, status);
        VL53L0X_SetXTalkCompensationEnable(pDevice, 0);
    }

    return true;
}
#include "debug_cli.h"
// 💡 센서가 정상적으로 응답하는지 체크하는 디텍트 함수
bool VL53L0X_Detect(void)
{
    bool condition_tof0 = false;
    app_config_t* app_config = get_app_config();
    // TOF0 조건 체크
    if (g_tof0_ok) {
        if (moving_average_get_L() > 60 && moving_average_get_L() < app_config->tof_sense_threshold_l) {
            condition_tof0 = true;
        }
    }

    DBG_Resister_t *DBG_Resister = Debug_Get();
    if(DBG_Resister->TOF)
    {
        if (g_tof0_ok) ESP_LOGI(TAG, "  [TOF0] Distance: %4d mm(raw = %4d)", moving_average_get_L() ,tof0_mm);
        else           ESP_LOGW(TAG, "  [TOF0] DISCONNECTED");
    }
    // ⭐️ 둘 중에 하나라도 조건을 만족(OR 연산)하면 true 반환, 둘 다 아니면 false 반환
    if (condition_tof0) {
        return true;
    } else {
        return false;
    }
}

// 💡 주기적으로 호출하여 센서 데이터를 읽어오는 함수 (100ms 등 태스크 내부 딜레이 주기 필수)
void VL53L0X_Sensing(void)
{
    VL53L0X_RangingMeasurementData_t measure_data0;
    VL53L0X_RangingMeasurementData_t measure_data1;
    uint32_t now_ms = xTaskGetTickCount() * portTICK_PERIOD_MS;

    // --- TOF0 (I2C 포트 0) 측정 ---
    if (g_tof0_ok) {
        VL53L0X_Error err0 = VL53L0X_PerformSingleRangingMeasurement(&dev_tof0, &measure_data0);
        if (err0 == VL53L0X_ERROR_NONE && measure_data0.RangeStatus == 0) {
            tof0_mm = measure_data0.RangeMilliMeter; // 아크릴 노이즈가 제거된 깨끗한 실제 거리
            ts_tof0_ms = now_ms;
            g_tof0_last_ok_ms = now_ms;
        } else if (measure_data0.RangeStatus == 2 || measure_data0.RangeStatus == 4) {
            // 허공이거나 너무 멀어서 측정이 안 된 경우 -> 장애물이 없다는 뜻!
            tof0_mm = 8190; // VL53L0X의 최대 에러 값(Out of Range) 대입 또는 무시
            // ESP_LOGD(TAG, "TOF0: 물체가 감지 범위를 벗어났습니다. (Status: %d)", measure_data0.RangeStatus);
        }
        else {
            ESP_LOGW(TAG, "TOF0 Range Status Warning: %d", measure_data0.RangeStatus);
        }
    }



    moving_average_update_l(tof0_mm);

}

bool TOF_VL53L0X_init(void)
{
    i2c_config_t i2c_bus0_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_TOF0_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_DISABLE,
        .scl_io_num = PIN_TOF0_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_DISABLE,
        .master.clk_speed = 100000, // 400kHz
    };
    
    if (i2c_param_config(I2C_NUM_0, &i2c_bus0_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to config I2C Port 0");
    }
    if (i2c_driver_install(I2C_NUM_0, i2c_bus0_cfg.mode, 0, 0, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install I2C Port 0");
    }

    //vTaskDelay(pdMS_TO_TICKS(100));

    // -------------------------------------------------------------
    // 3. ⭐️ ST 공식 C API를 이용한 하드웨어 엔진 캘리브레이션 구동 ⭐️
    // -------------------------------------------------------------
    ESP_LOGI(TAG, "Starting VL53L0X Pure C Initialization...");

    g_tof0_ok = init_single_vl53l0x(&dev_tof0, I2C_NUM_0, "TOF0_PORT0");

    if (g_tof0_ok) {
        ESP_LOGI(TAG, "🎉 양쪽 TOF 센서 모두 아크릴 보정 및 초기화 완벽 성공!");
    } else {
        ESP_LOGE(TAG, "⚠️ 일부 센서 초기화 실패 (TOF0: %s", 
                 g_tof0_ok ? "OK" : "FAIL");
    }

    return (g_tof0_ok);
}