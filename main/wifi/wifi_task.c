#include "string.h"
#include "wifi_task.h"
#include "esp_netif.h"

#include "protocol_examples_common.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "protocol_examples_common.h"
#include "example_common_private.h"
#include "mqtt_main.h"
#include "app_led.h"
#include "ble_task.h"
static const char* TAG = __FILE__;

#define USER_CONFIG_EXAMPLE_WIFI_SSID "iptime_lab0"
#define USER_CONFIG_EXAMPLE_WIFI_PASSWORD "gunpo!0929"
//#define USER_CONFIG_EXAMPLE_HOST_IP_ADDR CONFIG_EXAMPLE_IPV4_ADDR
#define USER_CONFIG_EXAMPLE_HOST_PORT CONFIG_EXAMPLE_PORT

#include <esp_https_ota.h>

#include <esp_ota_ops.h>
// 💡 메인 초기화 부분에 선언해두었던 이벤트 그룹과 비트들을 가져옵니다 (extern 또는 동일 파일 내 선언)
EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
static int s_retry_num = 0;
#define MAXIMUM_RETRY  5
static bool s_allow_reconnect = true; // 💡 자동 재연결 허용 플래그
/**
 * @brief 기존 Wi-Fi 연결을 안전하게 끊습니다.
 * (esp_wifi_stop()까지는 할 필요 없이 disconnect 만으로 충분합니다)
 */
void Wifi_Disconnect(void)
{
    wifi_ap_record_t ap_info;
    esp_err_t status = esp_wifi_sta_get_ap_info(&ap_info);

    if (status == ESP_OK) {
        ESP_LOGI(TAG, "현재 연결된 AP가 존재합니다. 연결을 해제합니다...");
        s_allow_reconnect = false;
        esp_wifi_disconnect();
        // 끊어질 시간을 잠깐 부여
        vTaskDelay(pdMS_TO_TICKS(500)); 
    } else {
        ESP_LOGI(TAG, "현재 연결된 AP가 없습니다. 해제 생략.");
    }
}

/**
 * @brief 새로운 SSID와 비밀번호로 Wi-Fi를 연결하고 결과를 기다립니다.
 */
void Wifi_Connect(const char* target_ssid, const char* target_password)
{
    led_bit_enable(PAIRING_BIT);

    // 1. 만약 어딘가 연결되어 있다면 먼저 끊어줍니다.
    Wifi_Disconnect();

    // 2. 새로운 설정을 담을 빈 구조체 생성
    wifi_config_t wifi_config = {0};

    // 3. 메모리 오버플로우를 막기 위해 안전한 strncpy 사용 (SSID 최대 32자, PASS 최대 64자)
    strncpy((char *)wifi_config.sta.ssid, target_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, target_password, sizeof(wifi_config.sta.password) - 1);

    ESP_LOGI(TAG, "새로운 AP로 연결 시도: SSID = %s", wifi_config.sta.ssid);

    // 4. 새로운 Wi-Fi 환경설정을 드라이버에 덮어씌웁니다.
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi 설정 적용 실패!");
        ble_send_data_to_queue((uint8_t*)"CONNECT_AP FAIL", strlen("CONNECT_AP FAIL"));
        led_bit_disable(PAIRING_BIT);
        return;
    }

    // 5. 이전에 남아있던 성공/실패 찌꺼기 비트를 깨끗하게 지웁니다.
    xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT);
    s_retry_num = 0;
    // 6. 연결 시작! (이벤트 핸들러가 백그라운드에서 동작하기 시작함)
    esp_wifi_connect();

    // 7. 결과가 나올 때까지 대기 (무한 대기 방지를 위해 최대 15초만 기다림)
    ESP_LOGI(TAG, "연결 결과를 기다리는 중...");
    EventBits_t bits = xEventGroupWaitBits(
        s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
        pdFALSE,
        pdFALSE,
        pdMS_TO_TICKS(15000) // 💡 15초(15000ms) 타임아웃 방어 로직
    );

    // 8. 대기 결과에 따른 처리
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "새 AP 연결 최종 성공!");
        ble_send_data_to_queue((uint8_t*)"CONNECT_AP SUCCESS", strlen("CONNECT_AP SUCCESS"));
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGE(TAG, "새 AP 연결 실패 (비밀번호 오류 또는 AP 없음)!");
        ble_send_data_to_queue((uint8_t*)"CONNECT_AP FAIL", strlen("CONNECT_AP FAIL"));
    } else {
        ESP_LOGE(TAG, "새 AP 연결 타임아웃! (15초 초과)");
        ble_send_data_to_queue((uint8_t*)"CONNECT_AP TIMEOUT", strlen("CONNECT_AP TIMEOUT"));
        esp_wifi_disconnect(); // 타임아웃 났으니 연결 시도 중단
    }

    led_bit_disable(PAIRING_BIT);
}
/*
void wifi_init_sta_static_ip(char* WIFI_SSID, char* WIFI_PASS)
{
    // 1. 기본 네트워크 인터페이스 초기화
 //   esp_netif_init();
    esp_event_loop_create_default();
    esp_netif_t *netif = esp_netif_create_default_wifi_sta();

    // 2. DHCP 클라이언트 중지
    esp_netif_dhcpc_stop(netif);

    // 3. 고정 IP 설정
    esp_netif_ip_info_t ip_info;
    ip_info.ip.addr = ipaddr_addr("192.168.0.61");
    ip_info.gw.addr = ipaddr_addr("192.168.0.1");
    ip_info.netmask.addr = ipaddr_addr("255.255.255.0");

    esp_netif_set_ip_info(netif, &ip_info);

    // 4. Wi-Fi 초기화 및 시작
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_wifi_init(&cfg);
    esp_wifi_set_mode(WIFI_MODE_STA);

    wifi_config_t wifi_config ;
    memcpy(wifi_config.sta.ssid,WIFI_SSID,sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password,WIFI_PASS,sizeof(wifi_config.sta.password));
//
    esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config);
    esp_wifi_start();
    esp_wifi_connect();

    ESP_LOGI(TAG, "Wi-Fi STA static IP setup done");
}*/

#define WIFI_MAX_VALUE 30
static wifi_ap_record_t ap_list[WIFI_MAX_VALUE];


uint16_t remove_duplicate_best_rssi(wifi_ap_record_t *list, uint16_t count)
{
    uint16_t new_count = 0;

    for(int i=0; i<count; i++)
    {
        int found = -1;

        for(int j=0; j<new_count; j++)
        {
            if(strcmp((char*)list[i].ssid,
                      (char*)list[j].ssid)==0)
            {
                found = j;
                break;
            }
        }


        if(found >= 0)
        {
            // 더 강한 신호로 교체
            if(list[i].rssi > list[found].rssi)
            {
                memcpy(&list[found],
                       &list[i],
                       sizeof(wifi_ap_record_t));
            }
        }
        else
        {
            memcpy(&list[new_count],
                   &list[i],
                   sizeof(wifi_ap_record_t));

            new_count++;
        }
    }

    return new_count;
}
#if 0
uint16_t wifi_scan_start(void)
{
// 3. Wi-Fi 스캔 설정 및 시작
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false, // 숨겨진 SSID도 스캔
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 0,
        .scan_time.active.max = 0
    };

    memset(ap_list,0,sizeof(ap_list));
    ap_count = 0;

    ESP_LOGI(TAG, "Wi-Fi 스캔 시작...");
    // true로 설정하면 스캔이 완료될 때까지 블로킹(대기)합니다.
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true)); 

    // 4. 스캔된 AP 개수 확인 및 리스트 가져오기
    uint16_t number = WIFI_MAX_VALUE; // 최대 가져올 AP 개수




    // ⭐ 순서 변경: 실제 발견된 총 개수를 먼저 확인합니다.
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));


    // 발견된 게 있다면 버퍼 크기(20) 내에서 레코드를 가져옵니다.
    if (ap_count > 0) {
        if (ap_count < number) {
            number = ap_count; // 실제 개수만큼만 가져오도록 제한
        }
        ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_list));
        number = remove_duplicate_best_rssi(ap_list, number);
        for (int i = 0; i < number; i++) {
            ESP_LOGI(TAG, "SSID: %s | RSSI: %d | 채널: %d", 
                    ap_list[i].ssid, ap_list[i].rssi, ap_list[i].primary);
        }
        ESP_LOGI(TAG, "총 %d 개의 AP를 찾았습니다.", number);
    }
    return number;
}
#else
uint16_t wifi_scan_start(void)
{
    // 스캔 설정
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 0,
        .scan_time.active.max = 0
    };
    led_bit_enable(PAIRING_BIT);
    // 전역/기존 버퍼 초기화
    memset(ap_list, 0, sizeof(ap_list));
    uint16_t total_found_count = 0; // 중복 제거 후 최종적으로 모은 AP 개수

    // ⭐️ [반복문 도입] 최대 3번 스캔 시도
    for (int scan_iter = 1; scan_iter <= 3; scan_iter++) {
        ESP_LOGI(TAG, "[스캔 %d회차] Wi-Fi 스캔 시작...", scan_iter);
        
        // 스캔 수행 (완료될 때까지 블로킹)
        esp_err_t ret = esp_wifi_scan_start(&scan_config, true);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_wifi_scan_start 실패: %d", ret);
            continue; // 실패 시 다음 회차 시도
        }

        // 이번 회차에 발견된 AP 총 개수 확인
        uint16_t current_scan_num = 0;
        esp_wifi_scan_get_ap_num(&current_scan_num);
        
        if (current_scan_num > 0) {
            // 이번 회차 데이터를 임시로 받아올 버퍼 선언
            wifi_ap_record_t *temp_records = malloc(sizeof(wifi_ap_record_t) * current_scan_num);
            if (temp_records == NULL) {
                ESP_LOGE(TAG, "메모리 할당 실패");
                break;
            }

            // 임시 버퍼에 이번 스캔 결과 채우기
            uint16_t fetch_num = current_scan_num;
            esp_wifi_scan_get_ap_records(&fetch_num, temp_records);

            // ⭐️ 기존에 이미 모아둔 ap_list 뒤에 새로 찾은 데이터 이어 붙이기
            // ap_list 버퍼가 넘치지 않도록 방어벽 설정 (WIFI_MAX_VALUE 이하로 제한)
            for (int i = 0; i < fetch_num; i++) {
                if (total_found_count < WIFI_MAX_VALUE) {
                    ap_list[total_found_count] = temp_records[i];
                    total_found_count++;
                } else {
                    break;
                }
            }

            // 임시 버퍼 해제
            free(temp_records);

            // ⭐️ 이어 붙인 전체 리스트에서 중복 제거 및 정렬 수행
            total_found_count = remove_duplicate_best_rssi(ap_list, total_found_count);
        }

        ESP_LOGI(TAG, "[스캔 %d회차 결과] 현재까지 중복 제거 후 수집된 AP: %d개 / 목표: %d개", 
                 scan_iter, total_found_count, WIFI_MAX_VALUE);

        // ⭐️ [조기 탈출 조건] 목표한 개수(WIFI_MAX_VALUE)를 다 채웠다면 3번 다 안 돌고 즉시 탈출!
        if (total_found_count >= WIFI_MAX_VALUE) {
            ESP_LOGI(TAG, "🎯 목표한 개수(%d개)를 모두 채워 스캔을 조기 종료합니다.", WIFI_MAX_VALUE);
            break; 
        }

        // 연속 스캔 시 칩과 환경에 무리가 안 가도록 잠시 쉬어줍니다 (예: 200ms)
        if (scan_iter < 3) {
            vTaskDelay(pdMS_TO_TICKS(200));
        }
    }
    char strbuf[100];
    
    ble_send_data_to_queue((uint8_t*)strbuf,sprintf((char*)strbuf,"SCAN %d",total_found_count));
    vTaskDelay(pdMS_TO_TICKS(200));
    
    // 🏁 최종 수집된 결과 로그 출력
    for (int i = 0; i < total_found_count; i++) {
        ESP_LOGI(TAG, "-> 최종 리스트 [%d] SSID: %s | RSSI: %d | 채널: %d", 
                 i, ap_list[i].ssid, ap_list[i].rssi, ap_list[i].primary);

            int len = snprintf(
                strbuf,
                sizeof(strbuf),
                "%d %s %d",
                i,
                ap_list[i].ssid,
                ap_list[i].rssi
            );

            ble_send_data_to_queue(
                (uint8_t*)strbuf,
                len
            );
    }
    ESP_LOGI(TAG, "최종 스캔 종료: 총 %d 개의 AP 확정", total_found_count);
    led_bit_disable(PAIRING_BIT);
    return total_found_count;
}
#endif
// 백그라운드 이벤트 핸들러
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // 💡 중요: 여기서 esp_wifi_connect()를 절대 호출하지 않습니다!
        // 드라이버가 준비 완료(Start) 되었다는 로그만 남깁니다.
        ESP_LOGI(TAG, "Wi-Fi 드라이버가 준비되었습니다 (STA_START).");
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
            if (s_allow_reconnect) {
                if (s_retry_num < MAXIMUM_RETRY) {
                    esp_wifi_connect();
                    s_retry_num++;
                    ESP_LOGI(TAG, "연결 실패, 재시도 중... (%d/%d)", s_retry_num, MAXIMUM_RETRY);
                } else {
                    xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
                    ESP_LOGE(TAG, "최종 연결 실패 (재시도 횟수 초과)");
                }
            }
    } 
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP 할당 완료: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
void wifi_init(void)
{
s_wifi_event_group = xEventGroupCreate();

    // 1. TCP/IP 스택 및 기본 이벤트 루프 초기화
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // 2. Wi-Fi 하드웨어 드라이버 초기화
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 3. 이벤트 핸들러 등록 (Wi-Fi 상태 변화 감지)
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // 4. Wi-Fi 모드를 STA(단말기)로 설정하고 드라이버 켜기
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "Wi-Fi 초기화 완료! (대기 또는 자동 연결 진행 중)");
}