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
// 필터 설정값 (상황에 맞게 조절)
#define FILTER_ALPHA   0.80f  // 0.0 ~ 1.0 (낮을수록 부드럽지만 반응이 느려짐)
#define DEADBAND_LIMIT 0.05f  // 이 값보다 작은 변화는 노이즈로 보고 무시 (단위: g 또는 kg)

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
#if 0
typedef struct  { uint32_t t; float w; }WSample;
#define BUFFER_SIZE  10

static WSample g_wbuf[BUFFER_SIZE];
static uint8_t g_wbuf_n = 0;
static uint8_t g_wbuf_i = 0;
static uint16_t hx711_cal_enable = 0;

static float raw_buffer[BUFFER_SIZE];
static int current_count = 0; // 현재 버퍼에 저장된 데이터 개수 (초기값 0)
// 1이면 "물 보충 중", 0이면 "대기/안정 상태"
int water_refill_flag = 0; 
float start_water = 0;
static uint32_t start_index = 30;
void insert_to_raw_buffer(float new_data)
{
    if (start_index) {
        start_index--;
        return;
    }
    // 1. 로드셀 신규 무게 데이터를 버퍼 맨 뒤에 삽입
    if (current_count < BUFFER_SIZE) {
        raw_buffer[current_count] = new_data;
        current_count++;
    } 
    else {
        memmove(&raw_buffer[0], &raw_buffer[1], (BUFFER_SIZE - 1) * sizeof(float));
        raw_buffer[BUFFER_SIZE - 1] = new_data;
    }

    // 데이터가 버퍼 크기만큼 쌓이기 시작하면 판정
    if (current_count >= BUFFER_SIZE) 
    {
        // =================================================================
        // 2. [물 보충 시작 판정] 
        // 최근 기준점(raw_buffer[0])보다 현재 무게(new_data)가 3.0g 이상 "실제 증가"했을 때!
        // (fabs를 제거하여 감소(-g)하는 음수 상황에는 절대로 반응하지 않습니다)
        // =================================================================
        if ((new_data - raw_buffer[0]) >= 3.0f) 
        {
            if (water_refill_flag == 0)
            {
                start_water = raw_buffer[0]; // 보충 시작 직전 무게 픽스
                water_refill_flag = 1; 
                ESP_LOGI(TAG, "💧 물 보충 시작 감지! (시작 무게: %.2fg, 현재: %.2fg)", start_water, new_data);
            }
        }

        // =================================================================
        // 3. [물 보충 종료 판정]
        // 물이 다 차서 더 이상 g이 안 늘어나고, 최근 10개 데이터가 안정화되었는지 검사
        // =================================================================
        if (water_refill_flag == 1) 
        {
            int check_count = (current_count < 10) ? current_count : 10;
            int stable_data_count = 0;

            // 최근 10개의 데이터 변화폭 검사
            for (int i = 0; i < check_count; i++) 
            {
                int index = current_count - 1 - i;
                
                // 현재 무게(new_data)와 최근 10개 측정값들의 차이가 3.0g 미만으로 정체되어 있다면
                // (즉, 물을 더 이상 붓지 않고 수위가 멈춰있다면)
                if (fabs(new_data - raw_buffer[index]) < 3.0f) 
                {
                    stable_data_count++;
                }
            }

            // 최근 10개 데이터가 모두 멈춰서 안정화되었고, 
            // 최종 무게가 시작 무게보다 큰 경우(+g)에만 물 보충 정상 완료 처리!
            if (stable_data_count == check_count) 
            {
                float total_refilled = new_data - start_water;

                if (total_refilled >= 10.0f) { // 최소 3g 이상 보충되었을 때만 성공 처리
                    ESP_LOGI(TAG, "물 보충 완료! (총 보충량: +%.2fg, 최종무게: %.2fg)", total_refilled, new_data);
                } else {
                    ESP_LOGW(TAG, "일시적 노이즈로 보충 취소됨");
                }

                water_refill_flag = 0; // 플래그 리셋
            }
        }
    }
}
static unsigned long millis() {
  return (unsigned long)(esp_timer_get_time() / 1000ULL);
}
// -------------------------
// HX711 bitbang
// -------------------------
static bool hx711_read_raw(int32_t* out_raw) {
  // Wait for ready: DOUT goes LOW
  uint32_t start = millis();
  uint32_t loop_count = 0;
  while (gpio_get_level(PIN_HX711_DOUT) == 1) {
    if (millis() - start > 50) return false;
        loop_count++;
    if (loop_count > 10) { 
        vTaskDelay(1); // 10번 이상 안 나오면 통신 스택(코어0)을 위해 양보
    } else {
        esp_rom_delay_us(10); // 처음에는 10us 단위로 촘촘하게 센서 확인
    }
  }

  uint32_t data = 0;
  // 24 bits
  for (int i = 0; i < 24; ++i) {
    gpio_set_level(PIN_HX711_SCK, 1);
    esp_rom_delay_us(1);
    data = (data << 1) | (gpio_get_level(PIN_HX711_DOUT) ? 1 : 0);
    gpio_set_level(PIN_HX711_SCK, 0);
    esp_rom_delay_us(1);
  }

  // Gain channel A 128: 1 extra pulse
  gpio_set_level(PIN_HX711_SCK, 1);
  esp_rom_delay_us(1);
  gpio_set_level(PIN_HX711_SCK, 0);
  esp_rom_delay_us(1);

  // sign extend 24-bit
  if (data & 0x800000) data |= 0xFF000000;
  *out_raw = (int32_t)data;
  //out_raw = -out_raw; // Invert to make positive weight positive value
  return true;
}
static bool calc_weight_g(const app_config_t* s, int32_t raw, float* out_g) {
  if (s->hx1_scale == 0.0f) return false;
  *out_g = ((float)raw - (float)s->hx1_offset) / s->hx1_scale;
  return true;
}

static void push_weight_sample(float w) {
  const size_t max_samples = sizeof(g_wbuf) / sizeof(g_wbuf[0]);
  g_wbuf[g_wbuf_i] = (WSample){ millis(), w };
  g_wbuf_i = (uint8_t)((g_wbuf_i + 1) % max_samples);
  if (g_wbuf_n < max_samples) g_wbuf_n++;
}

static float moving_average_calc(void) {
    if (g_wbuf_n == 0) return filtered_weight; // 데이터가 없으면 1번 필터값 그대로 반환

    float sum = 0.0f;
    for (uint8_t i = 0; i < g_wbuf_n; i++) {
        sum += g_wbuf[i].w;
    }
    return sum / (float)g_wbuf_n; // 최근 데이터의 평균값 반환
}

float loadcell_data_get(void) {
    // ⚠️ 기존의 filtered_weight 대신, 2차 필터까지 끝난 이동 평균 값을 리턴해 줍니다!
    return moving_average_calc(); 
}

bool check_weight_condition(void) {
    float avg_val = moving_average_calc();
    return (avg_val < 200.0f || avg_val <= -1.0f);
}

void HX711_cal_scale(float known_weight_g)
{
    app_config_t* app_config = get_app_config();
    
    // 1. 만약 영점(offset)이 0이거나 무게가 0 이하면 계산 불가
    if (known_weight_g <= 0.0f || app_config->hx1_offset == 0) {
        ESP_LOGE(TAG, "영점을 먼저 잡거나, 올바른 기준 무게를 입력하세요.");
        return;
    }

    int32_t raw = 0;
    int32_t raw_sum = 0;
    uint8_t count = 0;

    ESP_LOGI(TAG, "스케일 캘리브레이션 시작... (기준 무게: %.1f g)", known_weight_g);

    // 2. 현재 올라가 있는 물건의 Raw 값을 10번 읽어서 평균을 냄 (정확도 향상)
    for(int k = 0; k < 10; ++k) {
        if (hx711_read_raw(&raw)) {
            raw_sum += raw;
            count++;
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 100ms 대기
    }

    if (count > 0) {
        int32_t current_raw_avg = raw_sum / count;
        
        // 3. 순수하게 증가한 Raw 값 계산 (현재 평균 Raw - 영점 Raw)
        int32_t raw_diff = current_raw_avg - app_config->hx1_offset;

        // 4. 스케일(Scale) 값 계산 = (증가한 Raw 값) / (실제 무게)
        if (raw_diff != 0) {
            app_config->hx1_scale = (float)raw_diff / known_weight_g;
            
            // 5. 플래시 메모리에 저장 (영구 보존)
            app_nvs_save_set();
            
            ESP_LOGI(TAG, "스케일 설정 완료! 새로운 Scale 값: %.2f", app_config->hx1_scale);
        } else {
            ESP_LOGE(TAG, "Raw 값 변화가 없습니다. 물건이 안 올려져 있거나 센서 오류입니다.");
        }
    }
}

static void HX711_cal_process(void)
{
    float w = 0.0f;
    bool ok_raw;
    bool ok_w;

    int32_t raw = 0;
    int32_t raw_sum = 0;
    uint8_t count = 0;
    app_config_t* app_config = get_app_config();
    for(int k = 0; k < 10; ++k) {
        ok_raw = hx711_read_raw(&raw);
        ok_w = ok_raw && calc_weight_g(app_config, raw, &w);
        ESP_LOGI(TAG,"[Offset Calc-%d] Weight: %.2f g (raw: %d)\r\n", k, ok_w ? w : 0.0f, raw);
        if (ok_w) {
            raw_sum += raw;
            count++;
        }
        vTaskDelay(500 / portTICK_PERIOD_MS);    
    }

    app_config->case_raw_data = raw + 1000;
    app_config->hx1_offset = count > 0 ? (raw_sum / count) : 0;
    app_nvs_save_set();
     ESP_LOGI(TAG, "Tare offset set to %d\r\n", app_config->hx1_offset);
}
void HX711_cal_init(uint16_t cal)
{
    hx711_cal_enable = cal;
}

/**
 * @brief 로드셀 데이터를 입력받아 필터링을 수행하는 함수
 * @param raw_weight HX711 등에서 막 읽어온 가공되지 않은 무게값
 */
void loadcell_filter_update(float raw_weight) {
    // 1. 지수 이동 평균 계산
    float next_ema = (FILTER_ALPHA * raw_weight) + ((1.0f - FILTER_ALPHA) * filtered_weight);
    
    // 2. 데드밴드 적용 (이전 필터값과의 차이가 아주 미세하면 이전 값 고정)
    if (fabsf(next_ema - filtered_weight) > DEADBAND_LIMIT) {
        filtered_weight = next_ema;
    }
}

/**
 * @brief 필터링된 깨끗한 무게값을 가져오는 함수
 */
float loadcell_filter_get(void) {
    return filtered_weight;
}

void HX711_Sensing(void)
{
    app_config_t* app_config = get_app_config();
    if(hx711_cal_enable)
    {
      if(hx711_cal_enable == 1)
      {
        app_config->hx1_offset = 0;
        app_config->hx1_scale = 1000.0f;
        HX711_cal_process();
      }
      else
      {
        HX711_cal_scale((float)hx711_cal_enable);
      }
        hx711_cal_enable = 0;
    }
    else{
        float w = 0.0f;
        bool ok_raw;
        bool ok_w;
        int32_t raw = 0;
        int32_t raw_sum = 0;
        uint8_t count = 0;
        // HX711 weight
        ok_raw = hx711_read_raw(&raw);
        ok_w = ok_raw && calc_weight_g(app_config, raw, &w);

        if (ok_w) 
        {
            // 💡 1. 읽어온 날것의 무게(w)를 필터에 업데이트합니다.
            loadcell_filter_update(w);
            
            // 💡 2. 필터링을 거쳐 나온 깨끗한 최종 무게값을 가져옵니다.
            float clean_weight = loadcell_filter_get();
            
            // 💡 3. 기존 원형 버퍼(g_wbuf)에도 필터링된 값을 저장합니다.
            push_weight_sample(clean_weight);
            
            // 로그 출력 시 날것의 값(w)과 필터링된 값(clean_weight)을 함께 비교해 보세요.
            insert_to_raw_buffer(w);
        }
        
        float avg_val = moving_average_calc();
        DBG_Resister_t *DBG_Resister = Debug_Get();
        // 💡 구조체 멤버에서 안전하게 값을 지역 변수로 먼저 꺼냅니다.
        // 이렇게 하면 비교 시점에 메모리 불일치로 인한 오작동을 차단할 수 있습니다.
        float safe_min_threshold = (float)app_config->min_weight_threshold; 
        int32_t safe_case_raw = (int32_t)app_config->case_raw_data;

        // 디버깅 로그로 복사된 실제 값을 찍어서 200이 맞는지 확실히 검증합니다.
        //ESP_LOGI("DEBUG", "avg_val: %.2f | safe_min: %.2f | raw: %d | safe_case_raw: %d", 
                //avg_val, safe_min_threshold, raw, safe_case_raw);


        if(DBG_Resister->HX711)
        {
            ESP_LOGI(TAG, " Raw: %.2f g | Filtered: %.2f g (raw_bits: %d)\r\n", w, moving_average_calc(), raw);
        }
        // 💡 이제 안전한 로컬 변수끼리만 비교합니다.
        if (avg_val < safe_min_threshold) // 물부족
        {
            if(!led_bit_status(HARDWARE_ERR_BIT))
            {
                led_bit_enable(HARDWARE_ERR_BIT);
                water_fault_enable(WATER_LOW_FAULT);
                
            }       
        }       
        if(raw < safe_case_raw)//물그릇 탐지
        {
            if(!led_bit_status(HARDWARE_ERR_BIT))
            {
                led_bit_enable(HARDWARE_ERR_BIT);
                water_fault_enable(WATER_BOWL_DETACHED_FAULT);
            }

        }
        // 💡 3. 에러 해제 조건식도 안전한 로컬 변수로 교체합니다.
        // 흔들림 방지(히스테리시스)를 위해 임계값(200)보다 1g 큰 safe_min_threshold + 1.0f(즉, 201.0f)로 대칭을 맞춥니다.
        float safe_release_threshold = safe_min_threshold + 1.0f; 

        if(hardware_error_enable() && avg_val > safe_release_threshold && raw > safe_case_raw)
        {
            led_bit_disable(HARDWARE_ERR_BIT);
        }
    }
}


bool HX711_init(void)
{
    float w = 0.0f;
    bool ok_raw;
    bool ok_w;
// 1. PIN_HX711_DOUT 설정 (인풋 모드)
    gpio_config_t io_conf_dout = {
        .pin_bit_mask = (1ULL << PIN_HX711_DOUT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,       // HX711은 보통 외부에 풀업이 있거나 필요 없음
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE          // ⚠️ HX711 데이터 핀은 인터럽트를 쓰지 않습니다.
    };
    gpio_config(&io_conf_dout);

    // 2. PIN_HX711_SCK 설정 (아웃풋 모드)
    gpio_config_t io_conf_sck = {
        .pin_bit_mask = (1ULL << PIN_HX711_SCK),
        .mode = GPIO_MODE_OUTPUT,               // ⚠️ 아웃풋 모드로 변경
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&io_conf_sck);

    // 3. digitalWrite(PIN_HX711_SCK, LOW); 대체
    // 초기 시작 시 클럭 핀을 0(LOW) 상태로 안전하게 내려둡니다.
    gpio_set_level(PIN_HX711_SCK, 0);
    int32_t raw = 0;
    vTaskDelay(pdMS_TO_TICKS(500));
    return hx711_read_raw(&raw);
}
#else
#include "hx711_lib.h"
static int32_t hx711_data;
static int32_t hx711_data_buf;
#define CASE_WEIGHT 197700
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
    int32_t cal_data_buf = 0;
    for(int i=0;i<10;i++)
    {
        if(hx711_read_average(&dev, 10, &cal_data) == ESP_OK)
        {
            cal_data_buf+= cal_data;
        }
    }
   
    app_config->case_raw_data = cal_data_buf/10;
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
    if(app_config->case_raw_data == 0)
        case_weight = CASE_WEIGHT;
    else
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

    static uint8_t ucParameterToPass;
    TaskHandle_t xHandle = NULL;

    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            HX711_task,                  // 태스크 함수
            "HX711_task",                // 태스크 이름
            HX711_TASK_STACK_SIZE,       // 스택 크기
            &ucParameterToPass,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            &xHandle,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        ESP_LOGE(TAG, "Error creating Sensor_task on Core 1");
    }

    return true;
}
#endif



