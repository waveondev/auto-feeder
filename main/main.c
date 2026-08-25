/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
#include "nvs_flash.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_event.h"
#include "ble/ble_task.h"
#include "wifi_task.h"
#include "FreeRTOS_CLI.h"
#include "gpio_util.h"
#include "lwip/apps/mqtt.h"
#include "app_slid_motor.h"
#include "app_button.h"
#include "app_led.h"
#include "app_adc.h"
#include "opmode_task.h"

// AWS IoT Provisioning header 추가
#include "aws_iot_task.h"
#include "esp_spiffs.h"

extern void tcp_client(void);

#include "esp_vfs_dev.h"
#include "app_sensor.h"
#include "app_config_flash.h"
#include "motion_task.h"
#include "ble_tracker_id.h"
#include "isd2360.h"


//[by.jeon] 하드디스크(SPIFFS) 설정 및 초기화 함수
static esp_vfs_spiffs_conf_t spiffs_conf = {
  .base_path = "/spiffs",
  .partition_label = "spiffs_storage",
  .max_files = 5,
  .format_if_mount_failed = true
};

static void filesystem_init(void)
{
    ESP_LOGI("SPIFFS", "Initializing SPIFFS");
    esp_err_t ret = esp_vfs_spiffs_register(&spiffs_conf);
    if (ret != ESP_OK) {
        ESP_LOGE("SPIFFS", "Failed to mount or format filesystem");
        return;
    }
    ESP_LOGI("SPIFFS", "SPIFFS mounted successfully");
}
void check_reset_reason(void) {
    esp_reset_reason_t reason = esp_reset_reason();
    const char *TAG = "RESET_REASON";
    switch (reason) {
        case ESP_RST_POWERON:
            ESP_LOGI(TAG, "전원 켜짐 (Power-on reset)");
            break;
        case ESP_RST_SW:
            ESP_LOGI(TAG, "소프트웨어 재부팅 (esp_restart() 호출)");
            break;
        case ESP_RST_PANIC:
            ESP_LOGE(TAG, "크래시 발생 (Exception/Panic reset)");
            break;
        case ESP_RST_INT_WDT:
            ESP_LOGE(TAG, "인터럽트 워치독 작동 (Interrupt Watchdog)");
            break;
        case ESP_RST_TASK_WDT:
            ESP_LOGE(TAG, "태스크 워치독 작동 (Task Watchdog)");
            break;
        case ESP_RST_WDT:
            ESP_LOGE(TAG, "기타 워치독 작동 (Other Watchdog)");
            break;
        case ESP_RST_BROWNOUT:
            ESP_LOGW(TAG, "전압 강하 (Brownout reset - 전원 불안정)");
            break;
        case ESP_RST_SDIO:
            ESP_LOGI(TAG, "SDIO를 통한 리셋");
            break;
        default:
            ESP_LOGI(TAG, "기타 원인으로 인한 리셋 (코드: %d)", reason);
            break;
    }
}


void app_main(void) {
    // =========================================================================
    // 1️NVS (비휘발성 플래시 메모리) 초기화
    // AWS 프로비저닝 과정에서 발급받은 "고유 인증서"와 "개인키"를 
    // 기기의 플래시 메모리에 영구 저장하려면 NVS가 반드시 켜져 있어야 함.
    // =========================================================================
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS 파티션이 꼬였을 경우 포맷하고 다시 시도하는 방어 코드
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    check_reset_reason();
    NVS_Flash_init();

    filesystem_init();

    console_task_init();
    init_motor_ledc();
    button_task_init();

    LED_task_init();
    sensor_init();
    opmode_task_init();
    Create_Tracker_Capture_Task();
    ble_task_init();
    //isd2360_taskinit();


    wifi_init();
    aws_iot_task_init();
}
