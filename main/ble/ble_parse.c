#include "ble_parse.h"
#include "esp_mac.h" // MAC 주소 관련 API 헤더
#include "wifi_task.h"
#include "ble_task.h"
#include "app_config_flash.h"
#include "wifi_task.h"
#include "http_ota.h"
#define OTA_URL "https://evtago.s3.ap-northeast-2.amazonaws.com/water-dispenser.bin"
void BLE_APP_Command(uint8_t* data, uint16_t len)
{
    char buf[128];
    if(len >= sizeof(buf))
        len = sizeof(buf) - 1;

    memcpy(buf, data, len);
    buf[len] = '\0';


    // scan 명령
    if(strcmp(buf, "scan") == 0)
    {
        wifi_scan_start();
        return;
    }
    if(strcmp(buf, "OTA") == 0)
    {
        ota_main(OTA_URL);
        return;
    }


    char *cmd;
    char *index;
    char *ssid;
    char *pass;


    cmd = strtok(buf, " ");
    index = strtok(NULL, " ");
    ssid = strtok(NULL, " ");
    pass = strtok(NULL, " \r\n");


    printf("cmd   : %s\n", cmd ? cmd : "NULL");
    printf("index : %s\n", index ? index : "NULL");
    printf("ssid  : %s\n", ssid ? ssid : "NULL");
    printf("pass  : %s\n", pass ? pass : "NULL");


    if(cmd && strcmp(cmd, "CONNECT_AP") == 0)
    {
        if(ssid == NULL || pass == NULL)
        {
            printf("CONNECT_AP 파라미터 부족\n");
            return;
        }


        printf("SSID=%s PASS=%s\n", ssid, pass);


        wifi_config_t* wifi_config = get_wifi_config();


        memset(wifi_config->conn_ssid, 0,
            sizeof(wifi_config->conn_ssid));

        memset(wifi_config->conn_password, 0,
            sizeof(wifi_config->conn_password));


        strncpy((char*)wifi_config->conn_ssid,
                ssid,
                sizeof(wifi_config->conn_ssid)-1);

        strncpy((char*)wifi_config->conn_password,
                pass,
                sizeof(wifi_config->conn_password)-1);

        wifi_nvs_save_set();
        Wifi_Connect(ssid,pass);
        printf("저장 완료\n");
    }
}
static uint32_t total_count = 0;
static uint32_t input_count = 0;
void BLE_Receive_data(uint8_t* data, uint16_t len)
{
    Motion_Packet_t* Motion_Packet = (Motion_Packet_t*)data;
    
    printf("[BLE_Receive_data] %d 바이트 데이터 처리 중: ", len);

    for(int i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
    }
    printf("\n");
    printf("Code = %02x \r\n",Motion_Packet->event_code);
    switch(Motion_Packet->event_code)
    {   
        case MOTION_START_RESPONSE:
            printf("interval = %d Len = %d",Motion_Packet->motion_req.interval, Motion_Packet->motion_req.total_points);
            if(Motion_Packet->motion_req.total_points != 0)
            {
                    total_count = Motion_Packet->motion_req.total_points + (9-(Motion_Packet->motion_req.total_points%9));
                    input_count = 0;
            }
        break;
        case MOTION_DATA:
            input_count += 9;
            printf("seq = %d\n", Motion_Packet->motion_data.seq);

            printf("data0: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_0.bit.type, Motion_Packet->motion_data.pack_data_0.bit.data, Motion_Packet->motion_data.pack_data_0.word);
            printf("data1: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_1.bit.type, Motion_Packet->motion_data.pack_data_1.bit.data, Motion_Packet->motion_data.pack_data_1.word);
            printf("data2: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_2.bit.type, Motion_Packet->motion_data.pack_data_2.bit.data, Motion_Packet->motion_data.pack_data_2.word);
            printf("data3: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_3.bit.type, Motion_Packet->motion_data.pack_data_3.bit.data, Motion_Packet->motion_data.pack_data_3.word);
            printf("data4: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_4.bit.type, Motion_Packet->motion_data.pack_data_4.bit.data, Motion_Packet->motion_data.pack_data_4.word);
            printf("data5: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_5.bit.type, Motion_Packet->motion_data.pack_data_5.bit.data, Motion_Packet->motion_data.pack_data_5.word);
            printf("data6: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_6.bit.type, Motion_Packet->motion_data.pack_data_6.bit.data, Motion_Packet->motion_data.pack_data_6.word);
            printf("data7: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_7.bit.type, Motion_Packet->motion_data.pack_data_7.bit.data, Motion_Packet->motion_data.pack_data_7.word);
            printf("data8: type=%d, data=%d (word=%d)\n", Motion_Packet->motion_data.pack_data_8.bit.type, Motion_Packet->motion_data.pack_data_8.bit.data, Motion_Packet->motion_data.pack_data_8.word);
            if(input_count == total_count)
                motion_msg_send(MOTION_DATA_ACK,Motion_Packet->motion_data.seq);
        break;
        default:
            BLE_APP_Command(data,len);
        break;

    }
    


}
  //esp_read_mac(MyMac,ESP_MAC_WIFI_STA);
