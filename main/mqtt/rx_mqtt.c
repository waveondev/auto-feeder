#include "rx_mqtt.h"
#include "topic_list.h"
#include "mqtt_operations.h"
#include "stdio.h"
#include "esp_mac.h"

static const char *TAG = __FILE__;

static uint8_t topic_count = 0;
static char* topic_str[30];

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


    snprintf(sub_topic,sizeof(sub_topic),SERVER_RX_TOPIC_DRINK,dynamicMacStr);

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

    return true;
}
  














