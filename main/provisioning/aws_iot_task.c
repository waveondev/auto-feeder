#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h" // Wi-Fi 대기용
#include "esp_log.h"
#include "aws_iot_task.h"
#include "aws_iot_config.h"
#include "wifi_task.h"
#include "mqtt_operations.h"

#include "tx_mqtt.h"

// #include "core_mqtt.h"
// #include "fleet_provisioning_with_csr_demo.h" 

static const char *TAG = "aws_iot_task";
extern EventGroupHandle_t s_wifi_event_group;
static bool is_aws_started = false;
static QueueHandle_t mqtt_tx_queue = NULL;
static QueueHandle_t tracker_mqtt_queue = NULL;
static esp_timer_handle_t Health_timer;
#define AWS_IOT_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 4)
extern int aws_iot_provisioning_main( int argc, char ** argv );
// 15분 = 15분 * 60초 * 1,000,000us
#define TIMER_15_MIN_IN_US   (15ULL * 60ULL * 1000000ULL)

static void Health_timer_callback(void* arg)
{
    mqtt_queue_send(MESSEGE_HEALTH);
}

bool mqtt_queue_send(messege_tx_mqtt_cmd_e cmd)
{
    if(mqtt_tx_queue == NULL)
    {
        ESP_LOGW("mqtt_tx", "mqtt_tx_queue NULL.");
        return false;        
    }
    ESP_LOGW("mqtt_tx", "mqtt_tx_queue send.");
    if (xQueueSend(mqtt_tx_queue, &cmd, 0) != pdPASS) {
        ESP_LOGW("mqtt_tx", "Queue full! Dropping packet and freeing memory.");
        return false;
    }
    return true;
}
void tracker_mqtt_queue_send(messege_tx_mqtt_cmd_e cmd, uint8_t* mac, Motion_Packet_t* packet,uint32_t data_len,  pack_data* data )
{
    tracker_mqtt_packet_t mqtt_packet;

    if(tracker_mqtt_queue == NULL)
    {
        ESP_LOGW("mqtt_tx", "tracker_mqtt_queue NULL.");
        if(data != NULL)
            free(data);
        return;        
    }


    memset(&mqtt_packet,0,sizeof(tracker_mqtt_packet_t));
    mqtt_packet.cmd = cmd;
    memcpy(mqtt_packet.mac,mac,sizeof(mqtt_packet.mac));
    memcpy(&mqtt_packet.packet,packet,sizeof(Motion_Packet_t));
    mqtt_packet.data_len = data_len;
    mqtt_packet.data = data;
    ESP_LOGW("mqtt_tx", "tracker_mqtt_queue send.");
    if (xQueueSend(tracker_mqtt_queue, &mqtt_packet, 0) != pdPASS) {
        ESP_LOGW("mqtt_tx", "Queue full! Dropping packet and freeing memory.");
    }
}

static void aws_iot_main_entry(void *pvParameters)
{
    ESP_LOGI(TAG, "AWS IoT 전담 태스크 시작");
    // Wi-Fi 연결 대기 (IP 할당 확인)
    xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT,
        pdFALSE,
        pdTRUE,
        portMAX_DELAY
    );
    // -------------------------------------------------------------
    // 1. [1회성 초기화] 큐 및 타이머 생성을 루프 밖에서 단 1번만 수행
    // -------------------------------------------------------------
    mqtt_tx_queue = xQueueCreate(10, sizeof(messege_tx_mqtt_cmd_e));
    tracker_mqtt_queue = xQueueCreate(10, sizeof(tracker_mqtt_packet_t));

    const esp_timer_create_args_t Health_timer_args = {
        .callback = &Health_timer_callback,
        .name = "Health_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&Health_timer_args, &Health_timer));

    int i = 0;
    messege_tx_mqtt_cmd_e cmd;
    tracker_mqtt_packet_t mqtt_packet;

    // -------------------------------------------------------------
    // 2. [메인 재연결 루프]
    // -------------------------------------------------------------
    while(1)
    {
        // Wi-Fi 연결 대기 (IP 할당 확인)
        xEventGroupWaitBits(
            s_wifi_event_group,
            WIFI_CONNECTED_BIT,
            pdFALSE,
            pdTRUE,
            portMAX_DELAY
        );


        xQueueReset(mqtt_tx_queue);
        xQueueReset(tracker_mqtt_queue);

        // AWS IoT 프로비저닝 및 MQTT 연결
        while(aws_iot_provisioning_main(0, NULL) != EXIT_SUCCESS)
        {
            vTaskDelay(pdMS_TO_TICKS(3000)); /* 3초 대기 */
            i++;
            ESP_LOGI(TAG, "provisioning retry count = %d", i);
        }


        // 2) MQTT 연결이 붙었으니 헬스 타이머 동작 시작!
        esp_timer_start_periodic(Health_timer, TIMER_15_MIN_IN_US);

        ESP_LOGI(TAG, "=== MQTT 송수신 메인 루프 진입 ===");

        // MQTT 송수신 메인 루프
        for(;;) {
            if (xQueueReceive(mqtt_tx_queue, &cmd, pdMS_TO_TICKS(10)) == pdTRUE) {
                Send_cJSON_Messege(cmd);
            }
            if (xQueueReceive(tracker_mqtt_queue, &mqtt_packet, pdMS_TO_TICKS(10)) == pdTRUE) {
                Send_cJSON_Messege_for_tracker(&mqtt_packet);
            // ⭕ 훌륭함: 메시지 전송 처리 완료 후 동적 메모리 안전하게 해제
                if (mqtt_packet.data != NULL) {
                    free(mqtt_packet.data);
                    mqtt_packet.data = NULL; // Dangling Pointer 방지를 위해 NULL 처리 권장
                }
            }

            /* 수신 및 네트워크 연결 감시 */
            if (ProcessLoopWithTimeout(10) == false) {
                ESP_LOGE(TAG, "MQTT 연결 끊김 감지!");
                break; // for 루프 탈출
            }
        }

        // =========================================================
        // 🔴 [연결 끊김 시점]
        // =========================================================
        // 1) 네트워크가 끊겼으므로 헬스 타이머 즉시 정지!
        esp_timer_stop(Health_timer);

        // 2) 죽은 TLS/소켓 세션 정리
        DisconnectMqttSession();
        vTaskDelay(10000);
    }

    vTaskDelete(NULL);
}

void aws_iot_task_init(void)
{

    if (is_aws_started == false) {

        if (xTaskCreatePinnedToCore(
            aws_iot_main_entry,                  // 태스크 함수
            "aws_iot_task",                // 태스크 이름
            AWS_IOT_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 5,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
            ESP_LOGE(TAG, "Error creating aws_iot_task on Core 1");
        }


        //xTaskCreate(aws_iot_main_entry, "aws_iot_task", 24576, NULL, 5, NULL);
        is_aws_started = true;
    }
}