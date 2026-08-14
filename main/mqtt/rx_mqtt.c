#include "rx_mqtt.h"
#include "topic_list.h"
#include "mqtt_operations.h"
#include "stdio.h"
#include "esp_mac.h"
#include "cJSON.h"
#include "app_nvs.h"
static const char *TAG = __FILE__;

static uint8_t topic_count = 0;
static char* topic_str[30];
// 외부 BLE 암호화 전송 함수 선언
extern void ble_send_encrypted_event(const char *event_type, const char *plain_data);

char* topic_copy(char* str)
{
    if(topic_count >= 30)
        return NULL;

    uint8_t count = topic_count;
    size_t len = strlen(str);    
    topic_str[topic_count] = (char*)malloc(len+1);   
    if(topic_str[topic_count] == NULL)
        return NULL;
    memcpy(topic_str[topic_count],str,len+1);
    topic_count++;
    return topic_str[count];
}

bool mqtt_subscribe_init(void)
{
    char sub_topic[100];
    uint8_t mac_byte[6];
    char dynamicMacStr[13]; // 12자리 MAC 문자열 + 널 종료 문자(\0)
    const char * subTopic;
    bool subStatus;
    // ESP32의 기본 Wi-Fi MAC 주소를 읽어옵니다.
    esp_read_mac(mac_byte, ESP_MAC_WIFI_STA);
    
    snprintf(dynamicMacStr, sizeof(dynamicMacStr), "%02X%02X%02X%02X%02X%02X",
            mac_byte[0], mac_byte[1], mac_byte[2], mac_byte[3], mac_byte[4], mac_byte[5]);

    if(topic_count != 0)
    {
        for(int i=0;i<topic_count;i++)
            free(topic_str[i]);
        topic_count = 0;
    }

    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_REGISTRATION,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }
    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_BOOT,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }





    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_ACCESS,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }


    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_INTAKE,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_DISPENSE,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_DIAGNOSTICS,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_HEALTH,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),AWS_RX_TOPIC_JOBS_NOTIFY,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),AWS_RX_TOPIC_JOBS_NEXT_GET_ACCEPTED,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),AWS_RX_TOPIC_JOBS_GET_ACCEPTED,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),AWS_RX_TOPIC_JOBS_UPDATE_ACCEPTED,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }

    snprintf(sub_topic,sizeof(sub_topic),AWS_RX_TOPIC_SHADOW_DELTA,dynamicMacStr);

    subTopic = topic_copy(sub_topic);
    if(subTopic != NULL)
    {
        ESP_LOGI(TAG, "=== %d. 구독 === %s" , topic_count, subTopic);
        subStatus = SubscribeToTopic( subTopic, strlen( subTopic ) );

        if( subStatus != true )
        {
            ESP_LOGI(TAG, "=== %d. 구독 실패 === %s", topic_count, sub_topic);
        }
    }
    else
    {
        ESP_LOGI(TAG, "=== %d. 메모리 실패 === %s", topic_count, sub_topic);
    }
    return true;
}




static void registration_callback(cJSON* result)
{
    if(strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG,"[BLE_SEC] 백엔드 등록 성공 수신 완료! 앱에 prov_complete 전송");

        // MAC 주소를 동적으로 획득하여 thing_name 조립
        write_nvs_registration_flag( true );
        uint8_t mac_byte[6];
        char dynamicMacStr[13];
        esp_read_mac(mac_byte, ESP_MAC_WIFI_STA);
        snprintf(dynamicMacStr, sizeof(dynamicMacStr), "%02X%02X%02X%02X%02X%02X",
                mac_byte[0], mac_byte[1], mac_byte[2], mac_byte[3], mac_byte[4], mac_byte[5]);

        char payload_buf[128];
        snprintf(payload_buf, sizeof(payload_buf), "{\"thing_name\":\"%s_%s\"}",CONFIG_DEVICE_PREFIX, dynamicMacStr);

        // 3. 앱에 암호화된 최종 완료 통보 쏘기
        ble_send_encrypted_event("prov_complete", payload_buf);
    }
    else
    {
        ESP_LOGI(TAG, "[BLE_SEC] 백엔드 등록 실패"  );
    }
}

static void boot_callback(cJSON* result)
{
    if(strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG,"%s ok", result->valuestring);
    }
    else
    {
        ESP_LOGI(TAG,"other - %s ", result->valuestring);
    }
}

static void access_callback(cJSON* result)
{
    if(strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG,"%s ok", result->valuestring);
    }
    else
    {
        ESP_LOGI(TAG,"other - %s ", result->valuestring);
    }
}

static void diagnostics_callback(cJSON* result)
{
    if(strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG,"%s ok", result->valuestring);
    }
    else
    {
        ESP_LOGI(TAG,"other - %s ", result->valuestring);
    }
}

static void health_callback(cJSON* result)
{
    if(strcmp(result->valuestring, "ok") == 0) 
    {
        ESP_LOGI(TAG,"%s ok", result->valuestring);
    }
    else
    {
        ESP_LOGI(TAG,"other - %s ", result->valuestring);
    }
}


void mqtt_rx_messege(MQTTPublishInfo_t * pPublishInfo)
{
    if ( pPublishInfo->pPayload != NULL && pPublishInfo->payloadLength > 0 )
    {
        // 수신 데이터 파싱을 위해 임시 널 종료 문자 처리된 버퍼 생성
        char *temp_payload = malloc(pPublishInfo->payloadLength + 1);
        if (temp_payload != NULL)
        {
            memcpy(temp_payload, pPublishInfo->pPayload, pPublishInfo->payloadLength);
            temp_payload[pPublishInfo->payloadLength] = '\0';

            // JSON 파싱 시작
            cJSON *root = cJSON_Parse(temp_payload);
            if (root != NULL)
            {
                cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
                cJSON *result = cJSON_GetObjectItem(root, "result");
                // 1. 객체 존재 여부 + 타입이 문자열인지 + null 포인터가 아닌지 안전하게 검사
                if (cJSON_IsString(event_type) && (event_type->valuestring != NULL) &&
                    cJSON_IsString(result) && (result->valuestring != NULL)) 
                {
                    if (strcmp(event_type->valuestring, "registration") == 0)       
                    {
                        registration_callback(result);
                    }
                    else if (strcmp(event_type->valuestring, "boot") == 0)       
                    {
                        boot_callback(result);
                    }   
                    else if (strcmp(event_type->valuestring, "access") == 0)       
                    {
                        access_callback(result);
                    }      
                    else if (strcmp(event_type->valuestring, "diagnostics") == 0)       
                    {
                        diagnostics_callback(result);
                    }                               
                    else if (strcmp(event_type->valuestring, "health") == 0)       
                    {
                        health_callback(result);
                    }                                                   
                }
                cJSON_Delete(root);
            }
            free(temp_payload);
        }
    }
}

















