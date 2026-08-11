#include "app_led.h"
#include "led_strip.h"
#include "gpio_util.h"
#define LED_NUMBERS  6   // 연결된 네오픽셀 LED 총 개수 (예: 3개)
#define LED_BRIGHTNESS_MAX    255


#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "app_config_flash.h"
#include "opmode_task.h"

#include "app_TOF.h"
#include "debug_cli.h"
#include "app_button.h"
static led_strip_handle_t led_strip;
static const char *TAG = __FILE__;
#define LED_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)


static uint16_t led_status_resister = 0;
static int last_op_mode = -1; 
static bool wifi_conn_enable = false;
#define LED_TASK_DELAY 10
static uint8_t LED_brightness_value = LED_BRIGHTNESS_MAX;

typedef struct {
    uint8_t used;
    int8_t step;             // 밝기가 변화하는 공통 속도/스텝 수 (예: 5)
    
    uint8_t min_brightness;
    uint8_t max_brightness;
    // 목표하는 최대 색상 (상한선 기준값)
    uint8_t target_r;
    uint8_t target_g;
    uint8_t target_b;
    uint8_t target_w;
    int brightness;
} Breathing_Setting_t;

static Breathing_Setting_t Breathing_Setting;

void LED_Bright_Set(uint8_t value)
{
    LED_brightness_value = value;
}


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
bool food_empty_enable(void)
{
    return (led_status_resister & FOOD_EMPTY_BIT);
}
bool food_discharge_enable(void)
{
    return (led_status_resister & FOOD_DISCHARGE_BIT);
}
bool lock_mode_enable(void)
{
    return (led_status_resister & LOCK_MODE_BIT);
}
bool motor_mode_enable(void)
{
    uint16_t motor_bit = (7 << 5); // 5, 6, 7번 비트 마스크 (0x00E0)
    
    // 0이 아니면 하나라도 1이 켜져 있다는 의미
    return ((led_status_resister & motor_bit) != 0); 
}
void wifi_connect_success(void)
{
    wifi_conn_enable = true;
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
        //memset(&Breathing_Setting,0,sizeof(Breathing_Setting));
        ESP_LOGE(TAG, "led_status_resister = %08x",led_status_resister);
    }
}

void led_bit_disable(uint16_t disable)
{
    // 현재 마스터 버퍼에 해당 비트가 켜져 있을 때만 (즉, 새로 꺼지는 순간에만) 진입!
    if ((led_status_resister & disable) != 0) 
    {
        led_status_resister &= (~disable);
        //memset(&Breathing_Setting,0,sizeof(Breathing_Setting));
        ESP_LOGE(TAG, "led_status_resister = %08x",led_status_resister);
    }
}


void init_led_strip(void) {
    // 1. 네오픽셀 기본 설정 (v3.x 최신 규격)
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_RGB_GPIO,
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
void set_rgb_len_no_Breathing(uint8_t R, uint8_t G, uint8_t B, uint8_t W)
{
    if(Breathing_Setting.used)
        memset(&Breathing_Setting, 0, sizeof(Breathing_Setting_t));
    set_rgb_led(R, G, B, W);
}

void app_tof_sensor_poll_100ms(void)
{
    if (VL53L0X_Detect()) 
    {
        led_bit_enable(TOF_DETECT_BIT); 
    } 
    else 
    {
        led_bit_disable(TOF_DETECT_BIT); // ⭐️ 손 치우면 즉시 꺼짐 호출
    }
    app_config_t* app_config = get_app_config();
    if(last_op_mode != app_config->op_mode && led_status_resister == 0)
    {       
        last_op_mode = app_config->op_mode; 
         ESP_LOGE(TAG, "last_op_mode = %08x",last_op_mode);
    }
}

void Breathing_Setup(uint8_t enable, uint8_t step, 
                            uint8_t min_bright,  // 💡 최소 밝기 (0~255)
                            uint8_t max_bright,  // 💡 최대 밝기 (0~255)
                            uint8_t target_r,
                            uint8_t target_g,
                            uint8_t target_b,
                            uint8_t target_w)
{
    if(Breathing_Setting.used)
    {
        if(Breathing_Setting.target_r != target_r || Breathing_Setting.target_g != target_g || Breathing_Setting.target_b != target_b || Breathing_Setting.target_w != target_w)
        {
            Breathing_Setting.target_r = target_r;
            Breathing_Setting.target_g = target_g;
            Breathing_Setting.target_b = target_b;    
            Breathing_Setting.target_w = target_w;   
        }
        return;
    }
        

    memset(&Breathing_Setting, 0, sizeof(Breathing_Setting_t));
    Breathing_Setting.used = enable;
    Breathing_Setting.step = step;
    
    // 밝기 하한선/상한선 설정
    Breathing_Setting.min_brightness = min_bright;
    Breathing_Setting.max_brightness = max_bright;
    
    // 초기 시작 밝기를 min_brightness로 지정
    Breathing_Setting.brightness = min_bright;

    Breathing_Setting.target_r = target_r;
    Breathing_Setting.target_g = target_g;
    Breathing_Setting.target_b = target_b;    
    Breathing_Setting.target_w = target_w;   
}

void Breathing_Setup_Debug(uint8_t enable, uint8_t step, 
                            uint8_t min_bright,  // 💡 최소 밝기 (0~255)
                            uint8_t max_bright,  // 💡 최대 밝기 (0~255)
                            uint8_t target_r,
                            uint8_t target_g,
                            uint8_t target_b,
                            uint8_t target_w)
{


    memset(&Breathing_Setting, 0, sizeof(Breathing_Setting_t));
    Breathing_Setting.used = enable;
    Breathing_Setting.step = step;
    
    // 밝기 하한선/상한선 설정
    Breathing_Setting.min_brightness = min_bright;
    Breathing_Setting.max_brightness = max_bright;
    
    // 초기 시작 밝기를 min_brightness로 지정
    Breathing_Setting.brightness = min_bright;

    Breathing_Setting.target_r = target_r;
    Breathing_Setting.target_g = target_g;
    Breathing_Setting.target_b = target_b;    
    Breathing_Setting.target_w = target_w;   
}

static void Breathing_LED(void)
{
    if (Breathing_Setting.used == 0)
        return;

    // 1. 설정된 min ~ max 범위로 경계 제한
    if (Breathing_Setting.brightness > Breathing_Setting.max_brightness) 
        Breathing_Setting.brightness = Breathing_Setting.max_brightness;
    if (Breathing_Setting.brightness < Breathing_Setting.min_brightness) 
        Breathing_Setting.brightness = Breathing_Setting.min_brightness;

    // 2. 현재 밝기를 [0.0 ~ 1.0] 비율로 정규화
    float scale = (float)Breathing_Setting.brightness / 255.0f;

    // 3. 목표 색상에 비율 적용
    uint8_t r = (uint8_t)(Breathing_Setting.target_r * scale);
    uint8_t g = (uint8_t)(Breathing_Setting.target_g * scale);
    uint8_t b = (uint8_t)(Breathing_Setting.target_b * scale);
    uint8_t w = (uint8_t)(Breathing_Setting.target_w * scale);

    // 물리 LED에 반영
    set_rgb_led(r, g, b, w);

    // 4. 홀드 타임 처리
    static int hold_count = 0;
    if (hold_count > 0) {
        hold_count--;
        return;
    }

    // 5. 밝기 증감
    Breathing_Setting.brightness += Breathing_Setting.step;

    // 6. 설정한 max_brightness / min_brightness에서 방향 반전
    if (Breathing_Setting.brightness >= Breathing_Setting.max_brightness && Breathing_Setting.step > 0) {
        Breathing_Setting.brightness = Breathing_Setting.max_brightness;
        Breathing_Setting.step = -Breathing_Setting.step; // 부호 반전
        hold_count = 10;
    } 
    else if (Breathing_Setting.brightness <= Breathing_Setting.min_brightness && Breathing_Setting.step < 0) {
        Breathing_Setting.brightness = Breathing_Setting.min_brightness;
        Breathing_Setting.step = -Breathing_Setting.step; // 부호 반전
        hold_count = 10;
    }
}


static void LED_task(void *pvParameter)
{

    app_config_t* app_config = get_app_config();
    
    uint8_t toggle_time = 0;
    bool toggle_flag = false;

    init_led_strip();
    set_rgb_len_no_Breathing(0,0,0,LED_BRIGHTNESS_MAX);
    vTaskDelay(pdMS_TO_TICKS(5000));
    ESP_LOGI(TAG, "Starting LED_task (Pure Event Driven Mode)");
    DBG_Resister_t *DBG_Resister = Debug_Get();
    while (1) {
        app_tof_sensor_poll_100ms();
   
        if(DBG_Resister->led)
        {

        }
        else
        {
            int button_state = button_press_state();
            if(button_state)
            {
                switch(button_state) {
                    case 1: set_rgb_len_no_Breathing(0, LED_brightness_value, 0, 0); break;
                    case 2:  set_rgb_len_no_Breathing(0, 0, LED_brightness_value, 0);; break;
                    case 3:  set_rgb_len_no_Breathing(0, 0, 0, LED_brightness_value);; break;
                    default: break;
                }
            }
            else
            {
    // [우선순위 1] 특수 비트가 하나라도 켜져 있는 상태라면
                if (led_status_resister != 0) 
                {
                    last_op_mode = -1; // 모드 무효화
                    #if 1
                    if(hardware_error_enable() || sense_enable())
                    {
                        set_rgb_len_no_Breathing(LED_BRIGHTNESS_MAX,0, 0, 0); 
                    }
                    else 
                    #endif
                    if(food_empty_enable())
                    {
                        set_rgb_len_no_Breathing(LED_BRIGHTNESS_MAX,0, 0, 0); 
                    }
                    else if(motor_mode_enable())
                    {
                        Breathing_Setup(1,2,0,LED_brightness_value,0,0,0,255);
                        Breathing_LED();                        
                    }
                    else if(lock_mode_enable())
                    {
                        for(int count = 0; count < 2; count++)
                        {
                            set_rgb_len_no_Breathing(LED_brightness_value,0, 0, 0); 
                            vTaskDelay(250);
                            set_rgb_len_no_Breathing(0 ,0, 0, 0); 
                            vTaskDelay(250);
                        }
                        led_bit_disable(LOCK_MODE_BIT);
                    }
                    else if (pairing_enable()) {
                        Breathing_Setup(1,2,0,LED_brightness_value,0,0,255,0);
                        Breathing_LED();
                    }
                    else if (ota_enable()) {
                        Breathing_Setup(1,2,0,LED_brightness_value,255,0,255,0);
                        Breathing_LED();
                    }                  
                    else if (TOF_enable()){
                        set_rgb_len_no_Breathing(0, LED_brightness_value, 0, 0); 
                    }         
                }
                // [우선순위 2] 비트가 다 꺼진 정상 상태라면 op_mode 적용
                else 
                {
                    if(wifi_conn_enable == true)
                    {
                        wifi_conn_enable = false;
                        set_rgb_len_no_Breathing(0, LED_brightness_value, 0, 0); 
                        vTaskDelay(1000);
                    }
                    else
                    {

                        {
                            switch(last_op_mode) {
                                case FEED_MODE_SCHEDULED_PORTION: set_rgb_len_no_Breathing(0, 0, 0, LED_brightness_value); break;
                                case FEED_MODE_MANUAL_PORTION:  set_rgb_len_no_Breathing(0, 0, 0, LED_brightness_value); break;
                                case FEED_MODE_FREE_FEEDING:  set_rgb_len_no_Breathing(0,0 , LED_brightness_value, 0); break;
                                default: set_rgb_len_no_Breathing(0, 0, 0, LED_brightness_value); break;
                            }
                        }

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
// 3개 핀 비트마스크 구성 (1ULL << 핀번호)
    uint64_t pin_mask = (1ULL << LED_WHITE_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,             // 설정할 GPIO 핀 15, 16, 2 지정
        .mode = GPIO_MODE_OUTPUT,             // 출력 모드로 설정
        .pull_up_en = GPIO_PULLUP_DISABLE,    // 내부 풀업 비활성화
        .pull_down_en = GPIO_PULLDOWN_ENABLE, // 내부 풀다운 활성화 (기본 LOW 상태 유지)
        .intr_type = GPIO_INTR_DISABLE,       // 인터럽트 사용 안 함
    };

    // GPIO 설정 적용
    ESP_ERROR_CHECK(gpio_config(&io_conf));
    //gpio_set_level(LED_WHITE_GPIO,1);
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




