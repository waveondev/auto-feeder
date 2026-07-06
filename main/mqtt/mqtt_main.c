/*
 * SPDX-FileCopyrightText: 2022-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "mqtt_parse.h"
#include "esp_mac.h"
#include "mqtt_main.h"
#include "esp_log.h"

#include "ble/ble_parse.h"
#include "jbx-list.h"
static const char *TAG = "mqtt_main";
static esp_mqtt_client_handle_t client = NULL;
static uint8_t Tx_seq = 0;
static uint8_t MyMac[6];
static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

static esp_mqtt5_user_property_item_t user_property_arr[] = {
        {"board", "esp32"},
        {"u", "user"},
        {"p", "password"}
    };

#define USE_PROPERTY_ARR_SIZE   sizeof(user_property_arr)/sizeof(esp_mqtt5_user_property_item_t)

static esp_mqtt5_publish_property_config_t publish_property = {
    .payload_format_indicator = 1,
    .message_expiry_interval = 1000,
    .topic_alias = 0,
    .response_topic = "/topic/test/response",
    .correlation_data = "123456",
    .correlation_data_len = 6,
};

static esp_mqtt5_subscribe_property_config_t subscribe_property = {
    .subscribe_id = 25555,
    .no_local_flag = false,
    .retain_as_published_flag = false,
    .retain_handle = 0,
    .is_share_subscribe = true,
    .share_name = "group1",
};

static esp_mqtt5_subscribe_property_config_t subscribe1_property = {
    .subscribe_id = 25555,
    .no_local_flag = true,
    .retain_as_published_flag = false,
    .retain_handle = 0,
};

static esp_mqtt5_unsubscribe_property_config_t unsubscribe_property = {
    .is_share_subscribe = true,
    .share_name = "group1",
};

static esp_mqtt5_disconnect_property_config_t disconnect_property = {
    .session_expiry_interval = 60,
    .disconnect_reason = 0,
};

static void print_user_property(mqtt5_user_property_handle_t user_property)
{
    if (user_property) {
        uint8_t count = esp_mqtt5_client_get_user_property_count(user_property);
        if (count) {
            esp_mqtt5_user_property_item_t *item = malloc(count * sizeof(esp_mqtt5_user_property_item_t));
            if (esp_mqtt5_client_get_user_property(user_property, item, &count) == ESP_OK) {
                for (int i = 0; i < count; i ++) {
                    esp_mqtt5_user_property_item_t *t = &item[i];
                    ESP_LOGI(TAG, "key is %s, value is %s", t->key, t->value);
                    free((char *)t->key);
                    free((char *)t->value);
                }
            }
            free(item);
        }
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 *
 *  This function is called by the MQTT client event loop.
 *
 * @param handler_args user data registered to the event.
 * @param base Event base for the handler(always MQTT Base in this example).
 * @param event_id The id for the received event.
 * @param event_data The data for the event, esp_mqtt_event_handle_t.
 */
JBX_LIST(Mqtt_Tx_List);
#define TX_RETRY_COUNT 5
void mqtt_ack_input(uint8_t Cmd, uint8_t Seq)
{
   struct TxMqtt_queue* Ack_Mqtt_queue;
   for(Ack_Mqtt_queue = jbx_list_head(Mqtt_Tx_List);Ack_Mqtt_queue != NULL;Ack_Mqtt_queue = jbx_list_item_next(Mqtt_Tx_List))
   {
        if(Cmd == Ack_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Cmd && Seq == Ack_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Seq)
        {
                jbx_list_remove(Mqtt_Tx_List,Ack_Mqtt_queue);
                vPortFree(Ack_Mqtt_queue);
                break;
        }
   }

}


void mqtt_send_callback(TimerHandle_t xTimer)
{
   // ESP_LOGI(TAG,"Timer triggered!\n");
   struct TxMqtt_queue* Rx_Mqtt_queue;
    if(jbx_list_length(Mqtt_Tx_List) == 0)
        return;
    Rx_Mqtt_queue = jbx_list_head(Mqtt_Tx_List);

    esp_mqtt_client_publish(client, "/server/qos1", (char*)&Rx_Mqtt_queue->Mqtt_Queue.Mqtt_Header, (sizeof(Mqtt_packet_header_t) + Rx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Data_len), 1, 0);
    if(Rx_Mqtt_queue->Mqtt_Queue.count)
    {
        Rx_Mqtt_queue->Mqtt_Queue.count--;
    }
    else
    {
        jbx_list_remove(Mqtt_Tx_List,Rx_Mqtt_queue);
        vPortFree(Rx_Mqtt_queue);
    }
}
#include "esp_wifi.h"
void MQTT_Send(uint8_t ACK, uint8_t Direct, uint8_t* Seq, uint8_t CMD, uint8_t* data, uint32_t len)
{
    struct TxMqtt_queue* Tx_Mqtt_queue;
    uint8_t str[21];

    if(client == NULL)
        return;
    memset(str,0,sizeof(str));
    sprintf((char*)str,"/server/%02x%02x%02x%02x%02x%02x",MyMac[0],MyMac[1],MyMac[2],MyMac[3],MyMac[4],MyMac[5]);
    uint32_t alloc_len = 0; 
    alloc_len = (sizeof(struct TxMqtt_queue) + len);
    Tx_Mqtt_queue = pvPortMalloc(alloc_len);
    memset(Tx_Mqtt_queue,0,alloc_len);
    Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Cmd = CMD;
    if(ACK)
    {
        Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Cmd |= CHARGE_ACK;
    }
    if(Seq != NULL)
        Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Seq = *Seq;
    else
        Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Seq = Tx_seq++;
    memcpy(Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.mac,MyMac, 6);
    if(len)
    {
        memcpy(Tx_Mqtt_queue->Mqtt_Queue.data,data,len);
        Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Data_len = len;
    }
    wifi_ap_record_t ap_info;
    esp_wifi_sta_get_ap_info(&ap_info);
    Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Charge_State = 0;
    Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.rssi = ap_info.rssi;
    Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.RemainTime = (uint32_t)0;

    esp_mqtt_client_publish(client, (char*)str, (char*)&Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header, (sizeof(Mqtt_packet_header_t) + Tx_Mqtt_queue->Mqtt_Queue.Mqtt_Header.Data_len), 1, 0);
    if(ACK || Direct)
    {    
        vPortFree(Tx_Mqtt_queue);
    }
    else
    {
        Tx_Mqtt_queue->Mqtt_Queue.count = TX_RETRY_COUNT;
        jbx_list_add(Mqtt_Tx_List,Tx_Mqtt_queue);
    }
}
static void mqtt5_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t client = event->client;

    int msg_id;
    uint8_t str[20];
    Charge_Version_Get_t WakeUp;
    ESP_LOGD(TAG, "free heap size is %" PRIu32 ", minimum %" PRIu32, esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        print_user_property(event->property->user_property);
       // esp_mqtt5_client_set_user_property(&publish_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
        //esp_mqtt5_client_set_publish_property(client, &publish_property);
       // msg_id = esp_mqtt_client_publish(client, "/server/qos1", "data_3", 0, 1, 1);



        //esp_mqtt5_client_delete_user_property(publish_property.user_property);
        //publish_property.user_property = NULL;
        //ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
#if 0
        esp_mqtt5_client_set_user_property(&subscribe_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
        esp_mqtt5_client_set_subscribe_property(client, &subscribe_property);
        msg_id = esp_mqtt_client_subscribe(client, "/topic/qos0", 0);
        esp_mqtt5_client_delete_user_property(subscribe_property.user_property);
        subscribe_property.user_property = NULL;
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
#endif
        esp_mqtt5_client_set_user_property(&subscribe1_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
        esp_mqtt5_client_set_subscribe_property(client, &subscribe1_property);
        memset(str,0,sizeof(str));


        sprintf((char*)str,"/cp/%02x%02x%02x%02x%02x%02x",MyMac[0],MyMac[1],MyMac[2],MyMac[3],MyMac[4],MyMac[5]);
        msg_id = esp_mqtt_client_subscribe(client, (char*)str, 2);
        esp_mqtt5_client_delete_user_property(subscribe1_property.user_property);
        subscribe1_property.user_property = NULL;
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
#if 0
        esp_mqtt5_client_set_user_property(&unsubscribe_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
        esp_mqtt5_client_set_unsubscribe_property(client, &unsubscribe_property);
        msg_id = esp_mqtt_client_unsubscribe(client, "/topic/qos0");
        ESP_LOGI(TAG, "sent unsubscribe successful, msg_id=%d", msg_id);
        esp_mqtt5_client_delete_user_property(unsubscribe_property.user_property);
        unsubscribe_property.user_property = NULL;
#endif
       
        break;
    case MQTT_EVENT_DISCONNECTED:

        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        print_user_property(event->property->user_property);
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        print_user_property(event->property->user_property);    
        if(event->msg_id >= 0)
        {
            MQTT_Send(0,1,NULL, CHARGE_WAKEUP, (uint8_t*)&WakeUp, sizeof(WakeUp));
        }
        #if 0
        esp_mqtt5_client_set_publish_property(client, &publish_property);
        msg_id = esp_mqtt_client_publish(client, "/topic/qos0", "data", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        #endif
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        /*
        print_user_property(event->property->user_property);
        esp_mqtt5_client_set_user_property(&disconnect_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
        esp_mqtt5_client_set_disconnect_property(client, &disconnect_property);
        esp_mqtt5_client_delete_user_property(disconnect_property.user_property);
        disconnect_property.user_property = NULL;
        esp_mqtt_client_disconnect(client);
        */
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        print_user_property(event->property->user_property);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        print_user_property(event->property->user_property);
        ESP_LOGI(TAG, "payload_format_indicator is %d", event->property->payload_format_indicator);
        ESP_LOGI(TAG, "response_topic is %.*s", event->property->response_topic_len, event->property->response_topic);
        ESP_LOGI(TAG, "correlation_data is %.*s", event->property->correlation_data_len, event->property->correlation_data);
        ESP_LOGI(TAG, "content_type is %.*s", event->property->content_type_len, event->property->content_type);
        ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
        int 
        res = Mqtt_Messege_input((uint8_t*)event->data,event->data_len);
        if(res != 0)
            ESP_LOGI(TAG, "Mqtt_Messege_input= %d", res);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        print_user_property(event->property->user_property);
        ESP_LOGI(TAG, "MQTT5 return code is %d", event->error_handle->connect_return_code);
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

static void mqtt5_app_start(void)
{
    char *id_string = calloc(1, 32);
    
    sprintf((char*)id_string,"/server/%02x%02x%02x%02x%02x%02x",MyMac[0],MyMac[1],MyMac[2],MyMac[3],MyMac[4],MyMac[5]);
    char host_addr[128];
    sprintf(host_addr,"mqtt://%s:%d","192.168.0.55",4880);
    //#define URL_TEST "mqtt://jubix002.iptime.org:48890"
    esp_mqtt5_connection_property_config_t connect_property = {
        .session_expiry_interval = 10,
        .maximum_packet_size = 1024,
        .receive_maximum = 65535,
        .topic_alias_maximum = 2,
        .request_resp_info = true,
        .request_problem_info = true,
        .will_delay_interval = 10,
        .payload_format_indicator = true,
        .message_expiry_interval = 10,
        .response_topic = "/test/response",
        .correlation_data = "123456",
        .correlation_data_len = 6,
    };

    esp_mqtt_client_config_t mqtt5_cfg = {
        //.broker.address.uri = URL_TEST,
        .session.protocol_ver = MQTT_PROTOCOL_V_5,
        .network.disable_auto_reconnect = false,
        .credentials.username = "JBX",
        .credentials.authentication.password = "456",

        .session.last_will.msg = "i will leave",
        .session.last_will.msg_len = 12,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };
    mqtt5_cfg.broker.address.uri = host_addr;

    mqtt5_cfg.session.last_will.topic = (const char*)id_string;
#if CONFIG_BROKER_URL_FROM_STDIN
    char line[128];

    if (strcmp(mqtt5_cfg.uri, "FROM_STDIN") == 0) {
        int count = 0;
        printf("Please enter url of mqtt broker\n");
        while (count < 128) {
            int c = fgetc(stdin);
            if (c == '\n') {
                line[count] = '\0';
                break;
            } else if (c > 0 && c < 127) {
                line[count] = c;
                ++count;
            }
            vTaskDelay(10 / portTICK_PERIOD_MS);
        }
        mqtt5_cfg.broker.address.uri = line;
        printf("Broker url: %s\n", line);
    } else {
        ESP_LOGE(TAG, "Configuration mismatch: wrong broker url");
        abort();
    }
#endif /* CONFIG_BROKER_URL_FROM_STDIN */

    client = esp_mqtt_client_init(&mqtt5_cfg);

    /* Set connection properties and user properties */
    esp_mqtt5_client_set_user_property(&connect_property.user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
    esp_mqtt5_client_set_user_property(&connect_property.will_user_property, user_property_arr, USE_PROPERTY_ARR_SIZE);
    esp_mqtt5_client_set_connect_property(client, &connect_property);

    /* If you call esp_mqtt5_client_set_user_property to set user properties, DO NOT forget to delete them.
     * esp_mqtt5_client_set_connect_property will malloc buffer to store the user_property and you can delete it after
     */
    esp_mqtt5_client_delete_user_property(connect_property.user_property);
    esp_mqtt5_client_delete_user_property(connect_property.will_user_property);

    /* The last argument may be used to pass data to the event handler, in this example mqtt_event_handler */
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt5_event_handler, NULL);
    esp_mqtt_client_start(client);
    TimerHandle_t my_timer = xTimerCreate("mqtt_send_timer",(1000 / portTICK_PERIOD_MS), pdTRUE, 0, mqtt_send_callback);
    if(my_timer != NULL)
    {
        xTimerStart(my_timer, 0);  // 타이머 시작
        ESP_LOGE(TAG, "Timer Create");
    }
}
#include "esp_random.h"
esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type);
void mqtt_main(void)
{
    #if 0
    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) != ESP_OK) {
        ESP_LOGE("MQTT", "Wi-Fi 미연결로 MQTT를 시작할 수 없습니다.");
        return;
    }

    // 2. IP 주소 할당 여부 최종 확인
    esp_netif_ip_info_t ip_info;
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif == NULL || esp_netif_get_ip_info(netif, &ip_info) != ESP_OK || ip_info.ip.addr == 0) {
        ESP_LOGE("MQTT", "IP 주소가 아직 없어 MQTT를 시작할 수 없습니다.");
        return;
    }
    #endif
    esp_read_mac(MyMac,ESP_MAC_WIFI_STA);
//    esp_base_mac_addr_get(MyMac);
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("mqtt_client", ESP_LOG_VERBOSE);
    esp_log_level_set("mqtt_example", ESP_LOG_VERBOSE);
    esp_log_level_set("transport_base", ESP_LOG_VERBOSE);
    esp_log_level_set("esp-tls", ESP_LOG_VERBOSE);
    esp_log_level_set("transport", ESP_LOG_VERBOSE);
    esp_log_level_set("outbox", ESP_LOG_VERBOSE);

    esp_fill_random(&Tx_seq, 1); 
    jbx_list_init(Mqtt_Tx_List);
    mqtt5_app_start();
}
