# This gives OTA_INCLUDE_PUBLIC_DIRS, and OTA_SOURCES
include(${CMAKE_CURRENT_LIST_DIR}/aws-iot-core-mqtt-file-streams-embedded-c/mqttFileDownloaderFilePaths.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/../common/crypto/cryptoFilePaths.cmake)

set(AWS_OTA_PORT_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/port
    ${CMAKE_CURRENT_LIST_DIR}/../common/logging/
)

set(AWS_OTA_INCLUDE_DIRS
    ${MQTT_FILE_DOWNLOADER_INCLUDES}
    ${CMAKE_CURRENT_LIST_DIR}/config
    ${AWS_OTA_PORT_INCLUDE_DIRS}
)

set(AWS_OTA_PORT_SRCS
    ${CMAKE_CURRENT_LIST_DIR}/port/aws_esp_ota_ops.c
    ${CMAKE_CURRENT_LIST_DIR}/port/ota_pal.c
    ${CMAKE_CURRENT_LIST_DIR}/port/ota_os_freertos.c
    ${AWS_IOT_CRYPTO_SHARED_SRCS}
)

set(AWS_OTA_SRCS
    ${MQTT_FILE_DOWNLOADER_SOURCES}
    ${AWS_OTA_PORT_SRCS}
)

set(AWS_OTA_REQUIRES
    esp_rom
    mbedtls
    coreJSON
    esp_common
    spi_flash
    Jobs-for-AWS-IoT-embedded-sdk
    bootloader_support
    efuse
    log
    app_update
    espressif__cbor
)

set(AWS_OTA_PRIV_REQUIRES)
set(AWS_OTA_PRIV_INCLUDE_DIRS
    ${OTA_INCLUDE_PRIVATE_DIRS}
    ${AWS_IOT_CRYPTO_SHARED_INCLUDE_DIRS}
)

if(IDF_VERSION_MAJOR GREATER_EQUAL 6)
    list(APPEND AWS_OTA_PRIV_REQUIRES esp_hal_wdt)
endif()

idf_component_register(
    SRCS
        ${AWS_OTA_SRCS}
    INCLUDE_DIRS
        ${AWS_OTA_INCLUDE_DIRS}
    PRIV_INCLUDE_DIRS
        ${AWS_OTA_PRIV_INCLUDE_DIRS}
    PRIV_REQUIRES
        ${AWS_OTA_PRIV_REQUIRES}
    REQUIRES
        ${AWS_OTA_REQUIRES}
)

set_source_files_properties(
"${CMAKE_CURRENT_LIST_DIR}/aws-iot-core-mqtt-file-streams-embedded-c/source/MQTTFileDownloader.c"
PROPERTIES COMPILE_FLAGS -Wno-format)

if (NOT CONFIG_MQTT_STREAMS_USE_CUSTOM_CONFIG)
target_compile_definitions( ${COMPONENT_TARGET} PUBLIC MQTT_STREAMS_DO_NOT_USE_CUSTOM_CONFIG=1 )
endif()
