#include "app_led.h"
#include "led_strip.h"
#include "gpio_util.h"
#define LED_NUMBERS  6   // 연결된 네오픽셀 LED 총 개수 (예: 3개)
#define LED_BRIGHTNESS_MAX    180
#define LED_BRIGHTNESS_CENTER 100

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "app_config_flash.h"
#include "opmode_task.h"

#include "app_TOF.h"
#include "debug_cli.h"
static led_strip_handle_t led_strip;
static const char *TAG = __FILE__;
#define LED_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)


static uint16_t led_status_resister = 0;
static int last_op_mode = -1; 
static int wifi_conn_enable = 0;
#define LED_TASK_DELAY 10
typedef struct {
    uint8_t used;
    int8_t step;             // 밝기가 변화하는 공통 속도/스텝 수 (예: 5)
    
    // 각 채널의 현재 밝기 (Red는 0부터 시작, Blue는 255부터 시작)
    int16_t current_r;
    int16_t current_g;
    int16_t current_b;
    int16_t current_w;

    // 목표하는 최대 색상 (상한선 기준값)
    uint8_t target_r;
    uint8_t target_g;
    uint8_t target_b;
    uint8_t target_w;
    int brightness;
} Breathing_Setting_t;

static Breathing_Setting_t Breathing_Setting;
bool TOF_enable(void)
{
    return (led_status_resister & TOF_DETECT_BIT);
}
bool ota_enable(void)
{
    return (led_status_resister & OTA_START_BIT);
}
bool hardware_error_enable(void)
{
    return (led_status_resister & HARDWARE_ERR_BIT);
}
bool sense_enable(void)
{
    return (led_status_resister & SENSE_ERR_BIT);
}
bool pairing_enable(void)
{
    return (led_status_resister & PAIRING_BIT);
}
void wifi_connect_success(void)
{
    wifi_conn_enable = 100;
}
bool led_bit_status(uint16_t status)
{
    // 현재 마스터 버퍼에 해당 비트가 꺼져 있을 때만 (즉, 새로 켜지는 순간에만) 진입!
    if(led_status_resister & status)
    {
        return true;
    }

    return false;
}
void led_bit_enable(uint16_t enable)
{
    // 현재 마스터 버퍼에 해당 비트가 꺼져 있을 때만 (즉, 새로 켜지는 순간에만) 진입!
    if ((led_status_resister & enable) == 0) 
    {
        led_status_resister |= enable;
        memset(&Breathing_Setting,0,sizeof(Breathing_Setting));
        ESP_LOGE(TAG, "led_status_resister = %08x",led_status_resister);
    }
}

void led_bit_disable(uint16_t disable)
{
    // 현재 마스터 버퍼에 해당 비트가 켜져 있을 때만 (즉, 새로 꺼지는 순간에만) 진입!
    if ((led_status_resister & disable) != 0) 
    {
        led_status_resister &= (~disable);
        memset(&Breathing_Setting,0,sizeof(Breathing_Setting));
        ESP_LOGE(TAG, "led_status_resister = %08x",led_status_resister);
    }
}


void init_led_strip(void) {
    // 1. 네오픽셀 기본 설정 (v3.x 최신 규격)
    led_strip_config_t strip_config = {
        .strip_gpio_num = BLINK_GPIO,
        .max_leds = LED_NUMBERS,
        // ⚠️ v3.x에서는 아래와 같이 멤버명과 상수명이 바뀌었습니다!
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRBW, 
        .led_model = LED_MODEL_SK6812,
    };

    // 2. RMT 하드웨어 타이머 설정 (v3.x 최신 규격)
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT, // ⚠️ 클럭 소스를 명시적으로 지정해 주어야 합니다.
        .resolution_hz = 40 * 1000 * 1000, // 10MHz
        .mem_block_symbols = 64,
        .flags.with_dma = false,
    };

    // 3. ⚠️ v3.x 새 전용 초기화 함수 호출
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    set_led_clear();
}

void set_led_clear(void) {
    for(int i=0;i<LED_NUMBERS;i++)
    {
        led_strip_set_pixel_rgbw(led_strip, i, 0, 0, 0,0);
    }
    led_strip_refresh(led_strip); 
    
}


void set_rgb_led(uint8_t R, uint8_t G, uint8_t B, uint8_t W)
{
    static uint8_t R_buf, G_buf, B_buf, W_buf;
    if(R_buf == R && G_buf == G && B_buf == B && W_buf == W)
        return;
    R_buf = R;
    G_buf = G;
    B_buf = B;
    W_buf = W;

    for(int i=0;i<LED_NUMBERS;i++)
    {
         led_strip_set_pixel_rgbw(led_strip, i, R, G, B, W);
    }
    // 실제 SK6812 칩들로 32비트 정밀 신호 전송

    led_strip_refresh(led_strip); 
}

void app_tof_sensor_poll_100ms(void)
{
    static uint32_t tof_match_start_time = 0;
    static bool is_tof_pressing = false;

    if (VL53L0X_Detect()) 
    {

        is_tof_pressing = false;
        tof_match_start_time = 0;
        led_bit_enable(TOF_DETECT_BIT); 
    } 
    else 
    {
        led_bit_disable(TOF_DETECT_BIT); // ⭐️ 손 치우면 즉시 꺼짐 호출
        if (!is_tof_pressing) {
            tof_match_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            is_tof_pressing = true;
        } else {
            uint32_t elapsed_time = (xTaskGetTickCount() * portTICK_PERIOD_MS) - tof_match_start_time;
            if (elapsed_time >= 3000) {
                // ⭐️ 3초 만족 시 호출 -> 내부 가드 덕분에 매번 호출해도 세마포어는 딱 1번만 방출됨!
               
            }
        }
    }
    app_config_t* app_config = get_app_config();
    if(last_op_mode != app_config->op_mode && led_status_resister == 0)
    {       
        last_op_mode = app_config->op_mode; 
         ESP_LOGE(TAG, "last_op_mode = %08x",last_op_mode);
    }
}


static void Breathing_Setup(uint8_t enable, uint8_t step, 
                            int16_t current_r,
                            int16_t current_g,
                            int16_t current_b,
                            int16_t current_w,

                            // 목표하는 최대 색상 (상한선 기준값)
                            uint8_t target_r,
                            uint8_t target_g,
                            uint8_t target_b,
                            uint8_t target_w)
{
    if(Breathing_Setting.used)
        return;

    memset(&Breathing_Setting, 0,sizeof(Breathing_Setting_t));
    Breathing_Setting.used = enable;
    Breathing_Setting.step = step;
    Breathing_Setting.current_r = current_r;
    Breathing_Setting.current_g = current_g;
    Breathing_Setting.current_b = current_b;
    Breathing_Setting.current_w = current_w;
    Breathing_Setting.target_r = target_r;
    Breathing_Setting.target_g = target_g;
    Breathing_Setting.target_b = target_b;    
    Breathing_Setting.target_w = target_w;   

}
static void Breathing_LED(void)
{
    if(Breathing_Setting.used == 0)
        return;

    // 1. 제어 축 경계 제한
    if (Breathing_Setting.brightness > 255) Breathing_Setting.brightness = 255;
    if (Breathing_Setting.brightness < 0)   Breathing_Setting.brightness = 0;

// 2. 두 가지 비율 준비
    float forward = (float)Breathing_Setting.brightness / 255.0f; // 0.0 -> 1.0
    float backward = 1.0f - forward;                             // 1.0 -> 0.0

    // 3. ✨ [핵심 리팩토링] 시작 값이 255(최대)면 역방향비율을, 아니면 정방향비율을 곱함
    uint8_t r = (uint8_t)(Breathing_Setting.target_r * (Breathing_Setting.current_r >= 255 ? backward : forward));
    uint8_t g = (uint8_t)(Breathing_Setting.target_g * (Breathing_Setting.current_r >= 255 ? backward : forward));
    uint8_t b = (uint8_t)(Breathing_Setting.target_b * (Breathing_Setting.current_b >= 255 ? backward : forward));
    uint8_t w = (uint8_t)(Breathing_Setting.target_w * (Breathing_Setting.current_w >= 255 ? backward : forward));
    // 물리 LED에 반영
    // 물리 LED에 계산된 최종 값 반영
    set_rgb_led(r, g, b, w);

    // 4. 타이밍 및 스텝 증감 제어
    static int hold_count = 0;
    if (hold_count > 0) {
        hold_count--;
        return;
    }

    Breathing_Setting.brightness += Breathing_Setting.step;

    // step이 2든 5든 오버슛 걱정 없이 확실하게 반전되는 로직
    if (Breathing_Setting.brightness >= 255 && Breathing_Setting.step > 0) {
        Breathing_Setting.brightness = 255;
        Breathing_Setting.step = -Breathing_Setting.step; // 부호 반전
        hold_count = 10;
    } 
    else if (Breathing_Setting.brightness <= 0 && Breathing_Setting.step < 0) {
        Breathing_Setting.brightness = 0;
        Breathing_Setting.step = -Breathing_Setting.step; // 부호 반전
        hold_count = 10;
    }
}
static void LED_task(void *pvParameter)
{

    app_config_t* app_config = get_app_config();
    
    uint8_t toggle_time = 0;
    bool toggle_flag = false;
    static uint32_t _100ms_count = 0;

    init_led_strip();
    set_rgb_led(0,0,0,LED_BRIGHTNESS_MAX);
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "Starting LED_task (Pure Event Driven Mode)");
    DBG_Resister_t *DBG_Resister = Debug_Get();
    while (1) {
        if(_100ms_count >= (100 / LED_TASK_DELAY))
        {
            _100ms_count = 0 ;
            app_tof_sensor_poll_100ms();
        }
        else
            _100ms_count++;
        
   
        if(DBG_Resister->led)
        {

        }
        else
        {
            // [우선순위 1] 특수 비트가 하나라도 켜져 있는 상태라면
            if (led_status_resister != 0) {
                last_op_mode = -1; // 모드 무효화
                #if 1
                if((led_status_resister & HARDWARE_ERR_BIT) || (led_status_resister & SENSE_ERR_BIT))
                {
                    set_rgb_led(LED_BRIGHTNESS_MAX,0 , 0, 0); 
                }
                else 
                #endif
                if (led_status_resister & PAIRING_BIT) {
                    //Breathing_Setup(1,2,0,0,255,0,255,0,255,0);
                    Breathing_Setup(1,2,0,0,255,0,0,0,255,0);
                    Breathing_LED();
                }
                else if (led_status_resister & OTA_START_BIT) {
                    Breathing_Setup(1,2,0,0,255,0,255,0,255,0);
                    Breathing_LED();
                }                  
                else if (led_status_resister & TOF_DETECT_BIT){
                    set_rgb_led(0, LED_BRIGHTNESS_MAX, 0, 0); 
                }         
            }
            // [우선순위 2] 비트가 다 꺼진 정상 상태라면 op_mode 적용
            else {
                if(wifi_conn_enable)
                {
                    wifi_conn_enable--;
                    set_rgb_led(0, LED_BRIGHTNESS_MAX, 0, 0); 
                }
                else
                {
                    switch(last_op_mode) {
                        case FEED_MODE_SCHEDULED_PORTION: set_rgb_led(0, 0, 0, LED_BRIGHTNESS_MAX); break;
                        case FEED_MODE_MANUAL_PORTION:  set_rgb_led(0, 0, 0, LED_BRIGHTNESS_MAX/2); break;
                        case FEED_MODE_FREE_FEEDING:  set_rgb_led(0,0 , LED_BRIGHTNESS_MAX, 0); break;
                        default: set_rgb_led(0, 0, 0, LED_BRIGHTNESS_MAX); break;
                    }
                }

                
            // ESP_LOGE(TAG, "last_op_mode = %08x",last_op_mode);
            }
        }
        // ⭐️ [중요] 처리가 다 끝난 시점에 마스터 버퍼를 업데이트하여 다음 외부 진입을 방어합니다.
        vTaskDelay(pdMS_TO_TICKS(LED_TASK_DELAY));
        
    }
}






void LED_task_init(void)
{

    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            LED_task,                  // 태스크 함수
            "LED_task",                // 태스크 이름
            LED_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
        
        ESP_LOGE(TAG, "Error creating Button_task on Core 1");
    }
    
}




