#include "app_HX711.h"
#include "esp_system.h"
#include "esp_err.h"
#include "esp_log.h"
#include "gpio_util.h"
#include "FreeRTOS_CLI.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "app_config_flash.h"
#include "app_led.h"
#include "debug_cli.h"
static const char *TAG = __FILE__;
// 필터 설정값 (상황에 맞게 조절)
#define FILTER_ALPHA   0.60f  // 0.0 ~ 1.0 (낮을수록 부드럽지만 반응이 느려짐)
#define DEADBAND_LIMIT 0.05f  // 이 값보다 작은 변화는 노이즈로 보고 무시 (단위: g 또는 kg)

static float filtered_weight = 0.0f; // 현재 필터링된 최종 무게값
#include "math.h"
#if 0 
[SENSOR] Weight: 239.6 g (raw: 251934) 없을때
[SENSOR] Weight: 302.02 g (raw: 314222) 물통만 
[SENSOR] Weight: 347.7 g (raw: 346721) 모터+물통
[SENSOR] Weight: 379.8 g (raw: 366308) 전체

#endif
typedef struct  { uint32_t t; float w; }WSample;

static WSample g_wbuf[10];
static uint8_t g_wbuf_n = 0;
static uint8_t g_wbuf_i = 0;
static uint16_t hx711_cal_enable = 0;

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
            
        }
        
        float avg_val = moving_average_calc();
        DBG_Resister_t *DBG_Resister = Debug_Get();
        if(DBG_Resister->HX711)
        {
            ESP_LOGI(TAG, " Raw: %.2f g | Filtered: %.2f g (raw_bits: %d)\r\n", w, moving_average_calc(), raw);
        }
        if(avg_val < 200.0f || raw < app_config->case_raw_data)
        {

            led_bit_enable(HARDWARE_ERR_BIT);
        }
        if(hardware_error_enable() && avg_val > 201.0f && raw > app_config->case_raw_data)
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

