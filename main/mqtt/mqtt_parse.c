
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
#include "mqtt_main.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "ble/ble_parse.h"
#include "http_ota.h"

#define MQTT_PARSE_TAG "MQTT_PARSE"
esp_err_t esp_read_mac(uint8_t *mac, esp_mac_type_t type);
uint8_t Mqtt_Messege_input(uint8_t* data, uint32_t len)
{

    Mqtt_packet_t* Receve_Data = (Mqtt_packet_t*)data;
    uint8_t Tx_Data[256];
    uint8_t mac[6];
    uint32_t tx_len = 0;
    uint8_t Seq = 0;

    Charge_Version_Get_t* Version_Get;
    Charge_On_t* PowerOn;
    esp_read_mac(mac,ESP_MAC_WIFI_STA);
//    esp_base_mac_addr_get(mac);
    if(memcmp((char*)mac,(char*)Receve_Data->Mqtt_Header.mac,6) != 0)
        return 1;
    if(Receve_Data->Mqtt_Header.Cmd & CHARGE_ACK)
    {
        mqtt_ack_input(Receve_Data->Mqtt_Header.Cmd & 0x7F, Receve_Data->Mqtt_Header.Seq);
        return 2;
    }
    data[len] = 0;
    ESP_LOGI(MQTT_PARSE_TAG, "---------MQTT Parse IN Start------------");
    ESP_LOGI(MQTT_PARSE_TAG, "Len = %lu",len);
    ESP_LOG_BUFFER_HEX(MQTT_PARSE_TAG, data, len);
    ESP_LOGI(MQTT_PARSE_TAG, "----------MQTT Parse IN END-------------");

    Seq = Receve_Data->Mqtt_Header.Seq;
    MQTT_Send(1,0,&Seq,Receve_Data->Mqtt_Header.Cmd,Tx_Data,tx_len);

    return 0;
}



