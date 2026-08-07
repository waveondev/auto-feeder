#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "gpio_util.h"
#include "esp_log.h"
#include "app_config_flash.h"
#include "driver/gpio.h"
#include "app_slid_motor.h"
#include "opmode_task.h"
#include "wifi_task.h"
#include "ble_task.h"
#include "app_led.h"
#include "app_slid_motor.h"
static const char *TAG = "BUTTON_CTRL";
#define BUTTON_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

// 설정 타임아웃 (밀리초)
#define DEBOUNCE_TIME_MS      50
#define DOUBLE_CLICK_DELAY_MS 300
#define LONG_PRESS_3S_MS      3000
#define LONG_PRESS_5S_MS      5000
#define LONG_PRESS_10S_MS     10000

static bool button_lock = false;
// 이벤트 종류 정의
typedef enum {
    EV_SHORT_CLICK,
    EV_DOUBLE_CLICK,
    EV_LONG_3S,
    EV_LONG_5S,
    EV_LONG_10S
} button_event_t;

// 상태 관리를 위한 글로벌 변수들
static int click_count = 0;
static int64_t pressed_time = 0;
static bool is_pressed = false;

static QueueHandle_t gpio_evt_queue = NULL;
static TimerHandle_t double_click_timer = NULL;
// ⚠️ long_press_timer 및 long_press_stage 변수는 더 이상 필요 없으므로 삭제됨

// 함수 선언
static void Button_task(void* arg);
static void double_click_timer_callback(TimerHandle_t xTimer);

// [ISR] 인터럽트 서비스 루틴
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void bf_SingleClickAction(void) {
    //Opmode_Set();
    if(button_lock == true)
    {

    }
    else
    {
        feeder_mode_init();
    }
    ESP_LOGI(TAG,"Single Click Action executed \r\n");
}

void bf_DoubleClickAction(void) {
    app_config_t* app_config = get_app_config();
    Clean_mode_set();
    ESP_LOGI(TAG,"Double Click Action executed\r\n");
}

void bf_LongPress3SecAction(void) {
    if(button_lock == true)
        return;
    ESP_LOGI(TAG,"Long Press 3 Sec Action executed\r\n");
}

void bf_LongPress5SecAction(void) {
    if(button_lock == true)
        return;    
    ESP_LOGI(TAG,"Long Press 5 Sec Action executed \r\n");
    Wifi_Disconnect();
     wifi_scan_start();
}

void bf_LongPress10SecAction(void) {
    if(button_lock == true)
        return;
    ESP_LOGI(TAG,"Long Press 10 Sec Action executed \r\n");
    button_lock = !button_lock;
    //delay(1000); 
    //ESP.restart();
}
int button_press_state(void)
{
    if(button_lock == false)
    {
        if(is_pressed == true)
        {
            int64_t now = esp_timer_get_time() / 1000; // 현재 시간 (ms)
            int64_t press_duration = now - pressed_time; // 누르고 있던 총 시간 계산

            if (press_duration >= LONG_PRESS_10S_MS) {
                return 3;
            } 
            else if (press_duration >= LONG_PRESS_5S_MS) {
                return 2;
            } 
            else if (press_duration >= LONG_PRESS_3S_MS) {
                return 1;
            } 
        }
    }

    return 0;
}
// 버튼 상태 및 타이머를 처리하는 메인 태스크
static void Button_task(void* arg)
{
    uint32_t io_num;

    for(;;) {
        // 1. 인터럽트(눌림 또는 뗌) 발생 대기
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            
            // 2. 물리적 바운싱(지지직거림)이 진정될 때까지 딱 20ms만 멈춰서 기다림
            vTaskDelay(pdMS_TO_TICKS(20));
            
            // 3. 20ms를 기다리는 동안 큐에 추가로 쌓인 바운싱 노이즈(쓰레기 데이터)들 싹 비우기
            uint32_t dummy;
            xQueueReset(gpio_evt_queue);

            // 4. 노이즈가 모두 끝난 뒤의 '진짜 현재 버튼 상태'를 읽어옴
            int level = gpio_get_level(io_num);
            int64_t now = esp_timer_get_time() / 1000; // 현재 시간 (ms)

            // 버튼 눌림 (LOW) - 풀업 기준
            if (level == 0 && !is_pressed) {
                is_pressed = true;
                pressed_time = now;
            }
            // 버튼 떨어짐 (HIGH) - 뗐을 때 시간 판정!
            else if (level == 1 && is_pressed) {
                is_pressed = false;

                int64_t press_duration = now - pressed_time; // 누르고 있던 총 시간 계산

                // 가장 긴 시간부터 체크해서 걸러냅니다.
                if (press_duration >= LONG_PRESS_10S_MS) {
                    ESP_LOGI(TAG, "👉 [EVENT] 10초 롱클릭 (뗌)!");
                    bf_LongPress10SecAction();
                } 
                else if (press_duration >= LONG_PRESS_5S_MS) {
                    ESP_LOGI(TAG, "👉 [EVENT] 5초 롱클릭 (뗌)!");
                    bf_LongPress5SecAction();
                } 
                else if (press_duration >= LONG_PRESS_3S_MS) {
                    ESP_LOGI(TAG, "👉 [EVENT] 3초 롱클릭 (뗌)!");
                    bf_LongPress3SecAction();
                } 
                else {
                        click_count++;
                        xTimerChangePeriod(double_click_timer, pdMS_TO_TICKS(DOUBLE_CLICK_DELAY_MS), 0);
                        xTimerStart(double_click_timer, 0);
                }
            }
        }
    }
}
// 더블 클릭 대기 타이머 콜백
static void double_click_timer_callback(TimerHandle_t xTimer)
{
    if (click_count == 1) {
        ESP_LOGI(TAG, "👉 [EVENT] 단일 클릭 (Short)");
        bf_SingleClickAction();
    } else if (click_count >= 2) {
        ESP_LOGI(TAG, "👉 [EVENT] 더블 클릭!");
        bf_DoubleClickAction();
    }
    click_count = 0; // 카운트 초기화
}

void button_task_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << PIN_PKEY_STAT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE
    };
    gpio_config(&io_conf);
    
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIN_PKEY_STAT, gpio_isr_handler, (void*) PIN_PKEY_STAT);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    
    double_click_timer = xTimerCreate("double_click_timer", pdMS_TO_TICKS(DOUBLE_CLICK_DELAY_MS), 
                                      pdFALSE, (void*)0, double_click_timer_callback);
                                      
    if (xTaskCreatePinnedToCore(
            Button_task,
            "button_task",
            BUTTON_TASK_STACK_SIZE,
            NULL,
            tskIDLE_PRIORITY + 2,
            NULL,
            1
        ) != pdPASS) {
        ESP_LOGE(TAG, "Error creating Button_task on Core 1");
    }
}