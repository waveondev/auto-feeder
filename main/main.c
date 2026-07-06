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
#include "app_moter.h"
#include "app_button.h"
#include "app_led.h"
#include "app_adc.h"
#include "opmode_task.h"
extern void tcp_client(void);

#include "esp_vfs_dev.h"
#include "app_sensor.h"
#include "app_config_flash.h"
#include "motion_task.h"
#include "ble_tracker_id.h"
#include "isd2360.h"
void app_main(void)
{
    esp_err_t ret;
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
#if 0
        // TX 변환 끄기: \n 그대로 전송
    esp_vfs_dev_uart_set_tx_line_endings(ESP_LINE_ENDINGS_LF);
    esp_vfs_dev_uart_port_set_rx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,ESP_LINE_ENDINGS_LF);
    esp_vfs_dev_uart_port_set_tx_line_endings(CONFIG_ESP_CONSOLE_UART_NUM,ESP_LINE_ENDINGS_LF);
    // RX 쪽도 필요 시 조절 가능
    esp_vfs_dev_uart_set_rx_line_endings(ESP_LINE_ENDINGS_LF);
    ESP_ERROR_CHECK( ret );
#endif
    NVS_Flash_init();

    console_task_init();
    init_motor_ledc();
    button_task_init();
    init_led_strip();
    LED_task_init();
    if(sensor_init() == false)
        led_bit_enable(SENSE_ERR_BIT);

    //isd2360_taskinit();
    opmode_task_init();
    Create_Tracker_Capture_Task();
    ble_task_init();
    //charge_init();

    //mqtt_client_connect()
    //if(wifi_info_get_used())
    wifi_init();
}
