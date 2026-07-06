# 1. main 폴더 바로 아래와, 하위 폴더(ble, wifi, mqtt 등) 안의 모든 .c 파일을 자동으로 긁어모읍니다.
file(GLOB_RECURSE SOURCES "*.c")

if("${target}" STREQUAL "linux")
    set(requires esp_event esp_stubs protocol_examples_common nvs_flash)
endif()


# 컴포넌트 등록
idf_component_register(SRCS "isd2360.c" "opmode_task.c" "app_button.c" 
                            "wifi/http_ota.c" 
                            "wifi/wifi_task.c"     
                            "gpio_util.c"
                            "cli/config_cli.c" 
                            "cli/FreeRTOS_CLI.c" 
                            "cli/set_cli.c" 
                            "cli/debug_cli.c" 
                            "main.c" 
                            "app_moter.c"
                            "mqtt/mqtt_parse.c" 
                            "mqtt/mqtt_main.c" 
                            "jbx-list.c" 
                            "ble/ble_parse.c"   
                            "ble/ble_task.c"    
                            "ble/ble_tracker_id.c" 
                            "ble/ble_rx_task.c" 
                            "ble/ble_tx_task.c" 
                            "app_button.c"
                            "app_led.c"
                            "app_sensor.c"
                            "app_adc.c"
                            "app_HX711.c"
                            "app_TOF.c"
                            "app_nvs.c"
                            "app_config_flash.c"
                            "motion_task.c"
                            "isd2360.c"
                            "vl53l0x/core/src/vl53l0x_api.c"
                            "vl53l0x/core/src/vl53l0x_api_calibration.c"
                            "vl53l0x/core/src/vl53l0x_api_core.c"
                            "vl53l0x/core/src/vl53l0x_api_ranging.c"
                            "vl53l0x/platform/src/vl53l0x_platform.c"
                            INCLUDE_DIRS "." "ble" "wifi" "cli" "mqtt" "vl53l0x/core/inc" "vl53l0x/platform/inc"
                       # 블루투스, OTA, NVS flash에 더해 'driver'(GPIO용)까지 싹 다 명시합니다.

                       # 💡 esp_https_ota.h 에러를 해결하기 위해 여기에 명시적으로 등록합니다.
                        REQUIRES 
                            freertos 
                            esp_event 
                            esp_netif 
                            protocol_examples_common
                            esp_https_ota

                        PRIV_REQUIRES bt esp_https_ota nvs_flash app_update bootloader_support driver esp_driver_gpio vfs esp_driver_ledc led_strip esp_adc)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    "-DconfigGENERATE_RUN_TIME_STATS=1"
    "-DportGET_RUN_TIME_COUNTER_VALUE()=((unsigned_long)esp_timer_get_time())"
)