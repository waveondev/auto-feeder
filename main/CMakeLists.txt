# 1. main 폴더 바로 아래와, 하위 폴더(ble, wifi, mqtt, provisioning 등) 안의 모든 .c 파일을 자동으로 긁어모읍니다.
file(GLOB_RECURSE SOURCES "*.c")

if("${target}" STREQUAL "linux")
    set(requires esp_event esp_stubs protocol_examples_common nvs_flash)
endif()


# 컴포넌트 등록
idf_component_register(
    SRCS 
        ${SOURCES} # 이렇게 하면 새로 추가 되는 파일이 자동 인식 된다네요.
    
    INCLUDE_DIRS
        "."
        "${CMAKE_CURRENT_LIST_DIR}"
        "${CMAKE_CURRENT_LIST_DIR}/ble"
        "${CMAKE_CURRENT_LIST_DIR}/wifi"
        "${CMAKE_CURRENT_LIST_DIR}/cli"
        "${CMAKE_CURRENT_LIST_DIR}/mqtt"
        "${CMAKE_CURRENT_LIST_DIR}/provisioning"
        "${CMAKE_CURRENT_LIST_DIR}/vl53l0x/core/inc"
        "${CMAKE_CURRENT_LIST_DIR}/vl53l0x/platform/inc"
        "${CMAKE_CURRENT_LIST_DIR}/../components/common/posix_compat"
        "${CMAKE_CURRENT_LIST_DIR}/config"
        "${CMAKE_CURRENT_LIST_DIR}/crypto"
        "${CMAKE_CURRENT_LIST_DIR}/motor"
        "${CMAKE_CURRENT_LIST_DIR}/sensor"
        "${CMAKE_CURRENT_LIST_DIR}/gpio"
        "${CMAKE_CURRENT_LIST_DIR}/flash"
    # 블루투스, OTA, NVS flash에 더해 'driver'(GPIO용)까지 싹 다 명시합니다.
    # 💡 esp_https_ota.h 에러를 해결하기 위해 여기에 명시적으로 등록합니다.
    REQUIRES 
        freertos 
        esp_event 
        esp_netif 
        protocol_examples_common
        esp_https_ota
        spiffs

    PRIV_REQUIRES 
        bt 
        esp_https_ota 
        nvs_flash 
        app_update 
        bootloader_support 
        driver 
        esp_driver_gpio 
        vfs 
        esp_driver_ledc 
        led_strip 
        esp_adc 
        esp_timer
        json
        coreMQTT                                     
        coreJSON
        corePKCS11
        Fleet-Provisioning-for-AWS-IoT-embedded-sdk
        espressif__cbor
        backoffAlgorithm
        esp_netif
)

# target_compile_definitions(${COMPONENT_LIB} PRIVATE
#     "-DconfigGENERATE_RUN_TIME_STATS=1"
#     "-DportGET_RUN_TIME_COUNTER_VALUE()=((unsigned_long)esp_timer_get_time())"
# )