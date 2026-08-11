#include "ble_parse.h"
#include "esp_mac.h" // MAC 주소 관련 API 헤더
#include "wifi_task.h"
#include "ble_task.h"
#include "app_config_flash.h"
#include "wifi_task.h"
#include "http_ota.h"
#include "cJSON.h"         // ESP-IDF 내장 JSON 파서
#include "ble_crypto.h"    // BLE 암호화 모듈
#include "mbedtls/base64.h"
#include "mbedtls/gcm.h"
#include "esp_random.h"
#include "aws_iot_task.h"
#include "app_button.h"
#define OTA_URL "https://evtago.s3.ap-northeast-2.amazonaws.com/water-dispenser.bin"
// 분할 전송 시 사용할 MTU 사이즈를 저장할 전역/정적 변수 [앱에서 받을 수 있는 ble의 사이즈를 저장]

uint8_t* get_ble_session_key(void);
extern uint16_t g_ble_max_payload; // ble로 보낼 수 있는 MTU 사이즈 저장 변수

/**
 * @brief 암호화된 cJSON data 객체를 받아 복호화된 평문 문자열을 반환하는 함수
 * @param data_obj cJSON으로 파싱된 내측 data 오브젝트 ("iv", "ciphertext", "tag" 포함)
 * @return 복호화 성공 시 동적 할당된 평문 문자열 포인터 (사용 후 반드시 free 필요), 실패 시 NULL
 */
char* ble_decrypt_json_data(cJSON *data_obj) {
    if (!data_obj) return NULL;

    cJSON *iv_item = cJSON_GetObjectItem(data_obj, "iv");
    cJSON *ct_item = cJSON_GetObjectItem(data_obj, "ciphertext");
    cJSON *tag_item = cJSON_GetObjectItem(data_obj, "tag");

    if (!iv_item || !ct_item || !tag_item) {
        printf("[BLE_SEC] 에러: 필수 암호화 필드(iv/ciphertext/tag) 누락\n");
        return NULL;
    }

    // 1. Base64 디코딩을 위한 버퍼 준비
    unsigned char iv[16], tag[16];
    size_t iv_len, tag_len, ct_len;
    
    size_t b64_ct_len = strlen(ct_item->valuestring);
    unsigned char *ciphertext = (unsigned char *)malloc(b64_ct_len);
    unsigned char *plaintext = (unsigned char *)malloc(b64_ct_len + 1); // 널 종단 문자용 +1

    if (!ciphertext || !plaintext) {
        if (ciphertext) free(ciphertext);
        if (plaintext) free(plaintext);
        return NULL;
    }

    // Base64 문자열을 원본 바이너리로 변환
    mbedtls_base64_decode(iv, sizeof(iv), &iv_len, (const unsigned char *)iv_item->valuestring, strlen(iv_item->valuestring));
    mbedtls_base64_decode(tag, sizeof(tag), &tag_len, (const unsigned char *)tag_item->valuestring, strlen(tag_item->valuestring));
    mbedtls_base64_decode(ciphertext, b64_ct_len, &ct_len, (const unsigned char *)ct_item->valuestring, strlen(ct_item->valuestring));

    // 2. AES-256-GCM 복호화 진행
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    uint8_t* session_key = get_ble_session_key();
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, session_key, 256);

    // 복호화 수행 (성공 시 0 반환)
    int ret = mbedtls_gcm_auth_decrypt(&gcm, ct_len, iv, iv_len, NULL, 0, tag, tag_len, ciphertext, plaintext);
    
    mbedtls_gcm_free(&gcm);
    free(ciphertext); // 암호문 바이너리 버퍼는 여기서 바로 해제

    if (ret == 0) {
        plaintext[ct_len] = '\0'; // 평문을 안전한 문자열로 만들기
        return (char*)plaintext;  // 성공 시 복호화된 평문 문자열 반환 (외부에서 free 해야함)
    } else {
        printf("[BLE_SEC] 🚨 복호화 실패! 에러코드: -0x%04x\n", -ret);
        free(plaintext);
        return NULL;
    }
}

// JSON 데이터를 암호화하여 APP에 전송
void ble_send_encrypted_event(const char* event_type, const char* plain_data) {
    // 1. 랜덤 IV(초기화 벡터) 12바이트 생성
    unsigned char iv[12];
    esp_fill_random(iv, sizeof(iv)); 
    
    size_t plain_len = strlen(plain_data);
    unsigned char *ciphertext = (unsigned char *)malloc(plain_len);
    unsigned char tag[16]; // GCM 인증 태그

    // 2. AES-256-GCM 암호화 진행
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    
    uint8_t* session_key = get_ble_session_key();
    mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, session_key, 256);
    
    // 암호화 및 태그 생성
    mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plain_len, 
                              iv, sizeof(iv), NULL, 0, 
                              (const unsigned char*)plain_data, 
                              ciphertext, sizeof(tag), tag);
// 3. IV, Ciphertext, Tag를 Base64로 인코딩
    size_t iv_b64_len, ct_b64_len, tag_b64_len;
    unsigned char iv_b64[32] = {0}, tag_b64[32] = {0};
    
    // Base64 인코딩 결과물 크기는 보통 입력 크기의 약 1.33배에 여유 패딩을 주면 충분합니다.
    size_t ct_b64_alloc_len = plain_len * 2 + 16;
    unsigned char *ct_b64 = (unsigned char *)malloc(ct_b64_alloc_len); 
    if (ct_b64 == NULL) {
        free(ciphertext);
        mbedtls_gcm_free(&gcm);
        return;
    }

    mbedtls_base64_encode(iv_b64, sizeof(iv_b64), &iv_b64_len, iv, sizeof(iv));
    mbedtls_base64_encode(tag_b64, sizeof(tag_b64), &tag_b64_len, tag, sizeof(tag));
    mbedtls_base64_encode(ct_b64, ct_b64_alloc_len, &ct_b64_len, ciphertext, plain_len);
    size_t final_json_len = 70 + strlen(event_type) + iv_b64_len + ct_b64_len + tag_b64_len;

    char* final_json = (char *)malloc(final_json_len);
    if (final_json == NULL) {
        ESP_LOGE("BLE_SEC", "Failed to allocate memory for final_json!");
        free(ciphertext);
        free(ct_b64);
        mbedtls_gcm_free(&gcm);
        return;
    }

    snprintf(final_json, final_json_len, 
             "{\"event_type\":\"%s\",\"data\":{\"iv\":\"%s\",\"ciphertext\":\"%s\",\"tag\":\"%s\"}}", 
             event_type, iv_b64, ct_b64, tag_b64);

    // 5. 전송 큐에 넣기
    printf("[BLE_SEC] 암호화된 ACK 전송: %s\n", event_type);
    printf("[BLE_SEC] 암호화된 전송할 JSON Payload: %s\n", final_json);
    ble_send_data_to_queue(NULL,(uint8_t*)final_json, strlen(final_json));

    // 6. 메모리 정리
    free(ciphertext);
    free(ct_b64);
    free(final_json);
    mbedtls_gcm_free(&gcm);
}

void BLE_APP_Command(uint8_t* data, uint16_t len)
{
    char buf[256];
    if(len >= sizeof(buf))
        len = sizeof(buf) - 1;
    memcpy(buf, data, len);
    buf[len] = '\0';

    // ---------------------------------------------------------
    // 1. JSON 기반 프로토콜 파싱 (mtu_info, key_exchange 등)
    // ---------------------------------------------------------
    cJSON *root = cJSON_Parse(buf);
    if (root != NULL) {

        cJSON *event_type = cJSON_GetObjectItem(root, "event_type");
        printf("buf = %s \r\n",buf);
        if (event_type && cJSON_IsString(event_type)) {
            
            // [A] MTU 크기 (mtu_info) 수신
            if (strcmp(event_type->valuestring, "mtu_info") == 0) {
                cJSON *data_obj = cJSON_GetObjectItem(root, "data");
                if (data_obj) {
                    cJSON *max_payload = cJSON_GetObjectItem(data_obj, "max_payload");
                    if (max_payload && cJSON_IsNumber(max_payload)) {
                        g_ble_max_payload = max_payload->valueint;
                        printf("[BLE_SEC] MTU Negotiated: %d bytes\n", g_ble_max_payload);
                    }
                }
            } 
            // [B] 키 교환 (key_exchange) 수신
            else if (strcmp(event_type->valuestring, "key_exchange") == 0) {
                cJSON *data_obj = cJSON_GetObjectItem(root, "data");
                if (data_obj) {
                    cJSON *app_pub_key = cJSON_GetObjectItem(data_obj, "pub_key");
                    if (app_pub_key && cJSON_IsString(app_pub_key)) {
                        printf("[BLE_SEC] Received App PubKey: %s\n", app_pub_key->valuestring);

                        char dev_pub_key_b64[64] = {0};
                        
                        // 1. 디바이스 공개키 생성[cite: 7]
                        if (ble_crypto_init_and_get_pubkey(dev_pub_key_b64, sizeof(dev_pub_key_b64)) == ESP_OK) {
                            
                            // 2. 앱 공개키를 사용해 AES 세션키 도출
                            ble_crypto_compute_session_key(app_pub_key->valuestring);

                            // 3. 디바이스 공개키를 담아 key_exchange_ack 송신
                            char ack_buf[150];
                            snprintf(ack_buf, sizeof(ack_buf), 
                                     "{\"event_type\":\"key_exchange_ack\",\"data\":{\"pub_key\":\"%s\"}}", 
                                     dev_pub_key_b64);
                            
                            printf("[BLE_SEC] 전송할 JSON Payload: %s\n", ack_buf);

                            // 송신 큐를 이용해 앱으로 전송
                            ble_send_data_to_queue(NULL, (uint8_t*)ack_buf, strlen(ack_buf));
                            printf("[BLE_SEC] Sent Device PubKey & Handshake Complete.\n");
                        }
                    }
                }
            }
            // [C] 환경 설정 (set_env) 수신
            else if (strcmp(event_type->valuestring, "set_env") == 0) {
                printf("[BLE_DBG] set_env 블록 진입 성공 (암호화 데이터 처리)\n");

                cJSON *data_obj = cJSON_GetObjectItem(root, "data");
                char *plaintext = ble_decrypt_json_data(data_obj); 
    
                if (plaintext) {
                    printf("[BLE_SEC] 복호화 성공! 내용: %s\n", plaintext);

                    cJSON *decrypted_json = cJSON_Parse(plaintext);
                    if (decrypted_json) {
                        cJSON *env_val = cJSON_GetObjectItem(decrypted_json, "env");
                        
                        if (env_val && cJSON_IsString(env_val)) {
                            app_config_t* app_config = get_app_config();
                            
                            if (strcmp(env_val->valuestring, "dev") == 0) {
                                printf("[BLE_SEC] 환경 설정: 개발(dev) 모드\n");
                                strncpy(app_config->env_mode, "dev", sizeof(app_config->env_mode)-1);
                            } else if (strcmp(env_val->valuestring, "prod") == 0) {
                                printf("[BLE_SEC] 환경 설정: 운영(prod) 모드\n");
                                strncpy(app_config->env_mode, "prod", sizeof(app_config->env_mode)-1);
                            }
                            ble_send_encrypted_event("env_ack", "{\"result\":\"ok\"}");
                        }
                        cJSON_Delete(decrypted_json);
                    }
                    free(plaintext); // 사용이 끝난 평문 메모리는 반드시 해제!
                }
            }// [D] Scan Wi-Fi
            else if (strcmp(event_type->valuestring, "scan_wifi") == 0) {
                uint16_t ap_count = wifi_scan_start();
                printf("[BLE_SEC] 와이파이 스캔 완료! 총 %d 개 발견\n", ap_count);

                cJSON *data_obj = cJSON_CreateObject();
                cJSON *aps_array = cJSON_CreateArray();
                cJSON_AddItemToObject(data_obj, "aps", aps_array);

                //가장 신호가 좋은 최대 10개만 전송하자! 암호화로 데이터가 길어짐.
                uint16_t max_aps = (ap_count > 10) ? 10 : ap_count;

                for (int i = 0; i < max_aps; i++) {
                    cJSON *ap_item = cJSON_CreateObject();
                    
                    // 유나님의 ap_list 에서 데이터 추출!
                    cJSON_AddStringToObject(ap_item, "ssid", (char *)ap_list[i].ssid);
                    cJSON_AddNumberToObject(ap_item, "rssi", ap_list[i].rssi);
                    
                    // Auth 모드 변환
                    const char *auth_str = "open";
                    switch (ap_list[i].authmode) {
                        case WIFI_AUTH_WEP: auth_str = "wep"; break;
                        case WIFI_AUTH_WPA_PSK: auth_str = "wpa"; break;
                        case WIFI_AUTH_WPA2_PSK: 
                        case WIFI_AUTH_WPA_WPA2_PSK: auth_str = "wpa2"; break;
                        case WIFI_AUTH_WPA3_PSK:
                        case WIFI_AUTH_WPA2_WPA3_PSK: auth_str = "wpa3"; break;
                        default: auth_str = "open"; break;
                    }
                    cJSON_AddStringToObject(ap_item, "auth", auth_str);
                    
                    cJSON_AddItemToArray(aps_array, ap_item);
                }
                
                // 4. JSON 문자열로 변환
                char *plain_data = cJSON_PrintUnformatted(data_obj);
                if (plain_data) {
                    printf("[BLE_SEC] 암호화 전 스캔 결과(평문): %s\n", plain_data);
                    
                    // JSON String 암호화
                    ble_send_encrypted_event("wifi_list", plain_data);
                    free(plain_data);
                } else if (ap_count == 0) {
                    // 검색된 AP가 없을 때의 예외 처리
                    ble_send_encrypted_event("wifi_list", "{\"aps\":[]}");
                }
                
                cJSON_Delete(data_obj);

            }// [F] wifi_prov
            else if (strcmp(event_type->valuestring, "wifi_prov") == 0) {
                printf("[BLE_SEC] wifi_prov 블록 진입 성공 (와이파이 연결 처리)\n");

                cJSON *data_obj = cJSON_GetObjectItem(root, "data");
                char *plaintext = ble_decrypt_json_data(data_obj);
                
                if (plaintext) {
                    cJSON *prov_json = cJSON_Parse(plaintext);
                    if (prov_json) {
                        cJSON *ssid_val = cJSON_GetObjectItem(prov_json, "ssid");
                        cJSON *pwd_val = cJSON_GetObjectItem(prov_json, "pwd");

                        if (ssid_val && cJSON_IsString(ssid_val)) {
                            const char* ssid = ssid_val->valuestring;
                            const char* pwd = (pwd_val && cJSON_IsString(pwd_val)) ? pwd_val->valuestring : "";

                            printf("[BLE_SEC] 연결 시작 ➔ SSID: %s\n", ssid);
                            app_wifi_config_t* wifi_config = get_wifi_config();
                            memset(wifi_config->conn_ssid, 0,
                                sizeof(wifi_config->conn_ssid));
                            memset(wifi_config->conn_password, 0,
                                sizeof(wifi_config->conn_password));
                            strncpy((char*)wifi_config->conn_ssid,
                                    ssid,
                                    sizeof(wifi_config->conn_ssid)-1);
                            strncpy((char*)wifi_config->conn_password,
                                    pwd,
                                    sizeof(wifi_config->conn_password)-1);

                            // 유나님의 기존 연결 로직 구동
                            Wifi_Connect(ssid, pwd);

                            // 결과 판정 및 ACK 전송
                            EventBits_t bits = xEventGroupGetBits(s_wifi_event_group);
                            if (bits & WIFI_CONNECTED_BIT) {
                                wifi_nvs_save_set();
                                ble_send_encrypted_event("wifi_result", "{\"result\":\"ok\"}");
                            } else if (bits & WIFI_FAIL_BIT) {
                                ble_send_encrypted_event("wifi_result", "{\"result\":\"auth_fail\"}");
                            } else {
                                ble_send_encrypted_event("wifi_result", "{\"result\":\"timeout\"}");
                            }
                        }
                        cJSON_Delete(prov_json);
                    }
                    free(plaintext); // 💡 사용이 끝난 평문 메모리는 반드시 해제!
                }
            }
        }
        
        cJSON_Delete(root); // JSON 메모리 해제
        return; // JSON 처리 완료 시 함수 종료 (아래 텍스트 파싱 생략)
    }

    // scan 명령
    if(strcmp(buf, "scan") == 0)
    {
        wifi_scan_start();
        return;
    }
    if(strcmp(buf, "lock") == 0)
    {
        Lock_Set(1);
        return;
    }
    if(strcmp(buf, "unlock") == 0)
    {
        Lock_Set(0);
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


        app_wifi_config_t* wifi_config = get_wifi_config();


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

static pack_data* data_buffer;
void BLE_Receive_data(uint8_t* mac, uint8_t* data, uint16_t len)
{
    Motion_Packet_t* Motion_Packet = (Motion_Packet_t*)data;
    
    printf("[BLE_Receive_data] %d 바이트 데이터 처리 중: ", len);
    for(int i = 0; i < len; i++)
    {
        printf("%02X ", data[i]);
    }
    printf("\n");

    switch(Motion_Packet->event_code)
    {   
        case MOTION_START_RESPONSE:
            printf("interval = %d Len = %d",Motion_Packet->motion_req.interval, Motion_Packet->motion_req.total_points);                    
            printf("\n================ [ MOTION_START_RESPONSE ] ================\n");
            printf(" interval   : %d\n", Motion_Packet->motion_req.interval);
            printf(" total_points   : %d 개 \n",  Motion_Packet->motion_req.total_points);
            printf("\n=====================================================\n\n");
                        
            if(Motion_Packet->motion_req.total_points != 0)
            {
                    total_count = Motion_Packet->motion_req.total_points;
                    input_count = 0;
                    data_buffer = (pack_data*)calloc(total_count,sizeof(pack_data));
                    if(data_buffer == NULL)
                        break;
                    
                    Motion_Timer_Set(mac, true);
                    tracker_mqtt_queue_send(TRACKER_MESSEGE_ACTIVITY,mac, Motion_Packet,0,NULL);
            }

        break;
        case MOTION_DATA:
            if(data_buffer == NULL)
                break;
            // 각 멤버의 주소를 배열로 묶어줍니다.
            pack_data* input_ptrs[] = {
                &Motion_Packet->motion_data.pack_data_0,
                &Motion_Packet->motion_data.pack_data_1,
                &Motion_Packet->motion_data.pack_data_2,
                &Motion_Packet->motion_data.pack_data_3,
                &Motion_Packet->motion_data.pack_data_4,
                &Motion_Packet->motion_data.pack_data_5,
                &Motion_Packet->motion_data.pack_data_6,
                &Motion_Packet->motion_data.pack_data_7,
                &Motion_Packet->motion_data.pack_data_8
            };

            for (int i = 0; i < 9; i++) {
                if (input_count < total_count) { 
                        memcpy(&data_buffer[input_count], input_ptrs[i], sizeof(pack_data));
                        input_count++;
                } else {
                    // total_count를 초과하는 예외 데이터 처리 (경고 로그)
                    printf("Warning: Buffer full! input_count(%ld) >= total_count(%ld)\n", input_count, total_count);
                    break;
                }
            }

            //memcpy(&data_buffer[0], &Motion_Packet->motion_data.pack_data_0, sizeof(pack_data));
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
            
            
            if(input_count >= total_count)
            {
                Motion_Timer_Set(mac, false);
                tracker_mqtt_queue_send(TRACKER_MESSEGE_ACTIVITY,mac, Motion_Packet,total_count,data_buffer);
                data_buffer = NULL;
                input_count = 0;
                total_count = 0;    
                         
                motion_msg_send(get_conn_handle_by_mac(mac),MOTION_DATA_ACK,Motion_Packet->motion_data.seq);
            }
            else
                Motion_Timer_Set(mac, true);
        break;
        case HEALTH_DATA_RESPONSE:
                    // event_code 및 health_data_res 내부 구조체 포인터
                // health_data_res 구조체 직접 접근
    

                printf("\n================ [ Health Data Res ] ================\n");
                printf(" Event Code    : 0x%02X (%u)\n", Motion_Packet->event_code, Motion_Packet->event_code);
                printf(" Struct Size   : %d bytes (Expected: 19 bytes)\n", sizeof(Motion_Packet->health_data_res));
                printf(" Total Packet  : %d bytes (Expected: 20 bytes)\n", sizeof(Motion_Packet_t));
                printf("-----------------------------------------------------\n");
                printf(" Uptime        : %lu sec (%lu hours %lu min)\n", 
                        (unsigned long)Motion_Packet->health_data_res.uptime_sec, 
                        (unsigned long)(Motion_Packet->health_data_res.uptime_sec / 3600), 
                        (unsigned long)((Motion_Packet->health_data_res.uptime_sec % 3600) / 60));
                        
                printf(" Bat Level     : %u %%\n", Motion_Packet->health_data_res.Bat_Level);
                printf(" Bat Voltage   : %u (e.g. %u.%uV)\n", 
                        Motion_Packet->health_data_res.Bat_Voltage, Motion_Packet->health_data_res.Bat_Voltage / 10, Motion_Packet->health_data_res.Bat_Voltage % 10);
                        
                printf(" FW Version    : v%u.%u.%u\n", Motion_Packet->health_data_res.major, Motion_Packet->health_data_res.minor, Motion_Packet->health_data_res.patch);
                printf(" Target RSSI   : %d dBm\n", Motion_Packet->health_data_res.target_rssi);
                
                // 비트필드 fault_flag 상세 출력
                printf(" Fault Flag    : 0x%02X (Raw Byte)\n", Motion_Packet->health_data_res.fault_flag.byte);
                printf("  |- Bat Status  : %u\n", Motion_Packet->health_data_res.fault_flag.bit.Bat_Status);
                printf("  |- IMU Error   : %u\n", Motion_Packet->health_data_res.fault_flag.bit.IMU_Err);
                printf("  |- BLE Error   : %u\n", Motion_Packet->health_data_res.fault_flag.bit.BLE_Err);
                printf("  |- Storage Err : %u\n", Motion_Packet->health_data_res.fault_flag.bit.storage);
                printf("  |- Reset Reason: %u\n", Motion_Packet->health_data_res.fault_flag.bit.reset_reason);
                
                printf("\n=====================================================\n\n");
                tracker_mqtt_queue_send(TRACKER_MESSEGE_HEALTH,mac, Motion_Packet,0,NULL);
        break;
        default:
            BLE_APP_Command(data,len);
        break;

    }
    


}
  //esp_read_mac(MyMac,ESP_MAC_WIFI_STA);
