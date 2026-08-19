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


// ⭐️ ST 공식 API용 디바이스 구조체 전역 변수 선언 (두 채널 분리)
static VL53L0X_Dev_t dev_tof0;

static bool g_tof0_ok = false;
static uint16_t tof0_mm;

#define FILTER_SIZE 10
static uint32_t TOF_Buf_L[FILTER_SIZE] = {0};

static int buffer_idx_L = 0;

/**
 * @brief 새로운 데이터를 필터 버퍼에 추가하는 함수
 * @param new_value 새로 측정된 센서 값
 */
static void moving_average_update_l(uint16_t new_value) {
    if (buffer_idx_L < FILTER_SIZE) {
        // 1. 아직 10개가 다 안 찼으면 앞에서부터 순서대로(0, 1, 2...) 채움
        TOF_Buf_L[buffer_idx_L] = new_value;
        buffer_idx_L++;
    } else {
        // 2. 10개가 꽉 차면 memmove로 한 칸씩 밀고 맨 끝(9번 방)에 최신 데이터 넣음
        memmove(&TOF_Buf_L[0], &TOF_Buf_L[1], sizeof(uint32_t) * (FILTER_SIZE - 1));
        TOF_Buf_L[FILTER_SIZE - 1] = new_value;
    }
}
/**
 * @brief 현재 버퍼에 쌓인 데이터들의 평균값을 계산하여 반환하는 함수
 * @return float 최근 FILTER_SIZE 개수의 평균값
 */
uint16_t moving_average_get_L(void) {
// 예외 처리: 들어온 데이터가 없으면 0 반환 (0으로 나누기 에러 방지)
    if (buffer_idx_L == 0) {
        return 0;
    }

    uint32_t sum = 0;

    // 딱 현재 쌓여 있는 buffer_idx_L 개수만큼만 앞에서부터 순회하며 합산
    for (int i = 0; i < buffer_idx_L; i++) {
        sum += TOF_Buf_L[i];
    }

    return (uint16_t)(sum / buffer_idx_L);
}

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

    // ⭐️ 둘 중에 하나라도 조건을 만족(OR 연산)하면 true 반환, 둘 다 아니면 false 반환
    if (condition_tof0 || GetTracker_Id_active()) {
        return true;
    } else {
        return false;
    }
}

// 💡 주기적으로 호출하여 센서 데이터를 읽어오는 함수 (100ms 등 태스크 내부 딜레이 주기 필수)
void VL53L0X_Sensing(void)
{
    VL53L0X_RangingMeasurementData_t measure_data0;

    // --- TOF0 (I2C 포트 0) 측정 ---
    if (g_tof0_ok) {
        VL53L0X_Error err0 = VL53L0X_PerformSingleRangingMeasurement(&dev_tof0, &measure_data0);
        if (err0 == VL53L0X_ERROR_NONE && measure_data0.RangeStatus == 0) {
            tof0_mm = measure_data0.RangeMilliMeter; // 아크릴 노이즈가 제거된 깨끗한 실제 거리
        } else if (measure_data0.RangeStatus == 2 || measure_data0.RangeStatus == 4) {
            // 허공이거나 너무 멀어서 측정이 안 된 경우 -> 장애물이 없다는 뜻!
            tof0_mm = 8190; // VL53L0X의 최대 에러 값(Out of Range) 대입 또는 무시
            // ESP_LOGD(TAG, "TOF0: 물체가 감지 범위를 벗어났습니다. (Status: %d)", measure_data0.RangeStatus);
        }
        else {
            ESP_LOGW(TAG, "TOF0 Range Status Warning: %d", measure_data0.RangeStatus);
        }
        DBG_Resister_t *DBG_Resister = Debug_Get();
        if(DBG_Resister->TOF)
        {
            if (g_tof0_ok) ESP_LOGI(TAG, "  [TOF0] Distance: %4d mm(raw = %4d)", moving_average_get_L() ,tof0_mm);
            else           ESP_LOGW(TAG, "  [TOF0] DISCONNECTED");
        }
    }

    moving_average_update_l(tof0_mm);
}

bool TOF_VL53L0X_init(void)
{
    i2c_config_t i2c_bus0_cfg = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = PIN_TOF0_I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = PIN_TOF0_I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 100000, // 400kHz
    };
    
// 3개 핀 비트마스크 구성 (1ULL << 핀번호)
    uint64_t pin_mask = (1ULL << PIN_TOF0_INT);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };

    // GPIO 설정 적용
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    gpio_set_level(PIN_TOF0_INT,1);

    vTaskDelay(100);
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
        ESP_LOGI(TAG, "양쪽 TOF 센서 모두 아크릴 보정 및 초기화 완벽 성공!");
    } else {
        ESP_LOGE(TAG, "센서 초기화 실패 (TOF0: %s)", 
                 g_tof0_ok ? "OK" : "FAIL");
    }

    return (g_tof0_ok);
}
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
    if (GetIR_ADC()) {
       // ESP_LOGI(TAG,"ADC = %d",GetIR_ADC());
        return true;
    } else {
        return false;
    }
}


void VL53L0X_Sensing(void)
{
    gpio_set_level(PIN_TOF0_INT, 1);   
    vTaskDelay(pdMS_TO_TICKS(2));
    ADC_Sensing();
    vTaskDelay(pdMS_TO_TICKS(1));
    gpio_set_level(PIN_TOF0_INT, 0); 
   


    //ESP_LOGI(TAG, "io2 = %d ",gpio_get_level(2));
    //vTaskDelay(500);
}

bool TOF_VL53L0X_init(void)
{    
  
    gpio_config_t io_conf = {                   
        .pin_bit_mask =(1ULL << PIN_TOF0_INT),             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };
    gpio_config(&io_conf);
    gpio_set_level(PIN_TOF0_INT, 0); 

    //Test_init();
    
    return true;
}
#endif
