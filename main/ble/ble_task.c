#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "ble_task.h"
/* NimBLE 필수 헤더 */
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "console/console.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "app_config_flash.h"
#include "ble_parse.h"
#include "motion_task.h"
#include "ble_tracker_id.h"
#include "opmode_task.h"
#include "device_config.h"
static const char *TAG = __FILE__;

#define DEVICE_NAME "Wave_Peri2"
#define MY_UUID128_BASE(XX, YY) \
    BLE_UUID128_DECLARE(0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, \
                        0xF3, 0x93, 0xB5, 0xA3, YY, XX, 0x40, 0x6E)
// 1. 전형적인 Nordic UART 서비스(NUS) UUID 배열 구조로 재정의 (엔디안 주의)
// BLE 128비트 UUID는 역순(Little Endian)으로 배치됩니다.
// 중괄호 { } 를 제거하고 알맹이 데이터만 남겨둡니다.
#define BLE_SVC_UUID128_ARR   0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,  0x93,0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
#define BLE_CHR_1_UUID128_ARR 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,  0x93,0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
#define BLE_CHR_2_UUID128_ARR 0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0,  0x93,0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E

static const ble_uuid128_t nus_svc_uuid = BLE_UUID128_INIT(BLE_SVC_UUID128_ARR);
static const ble_uuid128_t nus_rx_uuid  = BLE_UUID128_INIT(BLE_CHR_1_UUID128_ARR);
static const ble_uuid128_t nus_tx_uuid  = BLE_UUID128_INIT(BLE_CHR_2_UUID128_ARR);

static uint8_t own_addr_type;
static uint16_t ble_spp_svc_gatt_notify_val_handle;
static bool conn_handle_subs[CONFIG_BT_NIMBLE_MAX_CONNECTIONS + 1];
static esp_timer_handle_t Mac_sending_timer;



extern void ble_store_config_init(void);
static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg);

static uint16_t current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static bool is_phone_connected = false;
static QueueHandle_t ble_rx_queue = NULL;
static QueueHandle_t ble_tx_queue = NULL; // 이름을 수신용(rx)에서 송신용(tx) 개념으로

uint16_t g_ble_max_payload = 20; // ble로 최대 보낼 수 있는 Length 저장
#define BLE_TRX_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)

static void ble_spp_server_print_conn_desc(struct ble_gap_conn_desc *desc)
{
    MODLOG_DFLT(INFO, "handle=%d our_ota_addr_type=%d our_ota_addr=", desc->conn_handle, desc->our_ota_addr.type);
    MODLOG_DFLT(INFO, " peer_id_addr_type=%d peer_id_addr=", desc->peer_id_addr.type);
    MODLOG_DFLT(INFO, " encrypted=%d authenticated=%d bonded=%d\n", 
                desc->sec_state.encrypted, desc->sec_state.authenticated, desc->sec_state.bonded);
}

/**
 * [추가] 주변 비콘 스캔 시작 함수
 */
static void ble_spp_client_scan(void)
{
    struct ble_gap_disc_params disc_params;
    int rc;

    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.filter_duplicates = 0; 
    disc_params.passive = 0;           // Active 스캔 필수 (이름 요청용)

    // 🔴 [수정] 500ms 비콘 스캔을 위한 타이밍 최적화
    // interval과 window를 같게 설정하면 ESP32가 쉬지 않고 100% 확률로 계속 스캔 대기를 합니다.
    disc_params.itvl = 400;            // 스캔 주기 (단위: 0.625ms, 즉 250ms)
    disc_params.window = 400;          // 스캔 윈도우 (단위: 0.625ms, 즉 250ms) -> 100% 듀티 사이클

    rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, ble_spp_server_gap_event, NULL);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "Error discovering peers; rc=%d\n", rc);
    }
}
static void ble_spp_server_advertise(void)
{
    struct ble_gap_adv_params adv_params;
    struct ble_hs_adv_fields fields;
    struct ble_hs_adv_fields rsp_fields;
    int rc;

    // Advertising packet
    memset(&fields, 0, sizeof fields);

    fields.flags =
        BLE_HS_ADV_F_DISC_GEN |
        BLE_HS_ADV_F_BREDR_UNSUP;

    // ⭐ NUS 서비스 UUID 광고
    fields.uuids128 = &nus_svc_uuid;
    fields.num_uuids128 = 1;
    fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        return;
    }


    // Scan Response packet
    memset(&rsp_fields, 0, sizeof rsp_fields);

    const char *name = ble_svc_gap_device_name();

    rsp_fields.name = (uint8_t *)name;
    rsp_fields.name_len = strlen(name);
    rsp_fields.name_is_complete = 1;


    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        return;
    }


    memset(&adv_params, 0, sizeof adv_params);

    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;


    rc = ble_gap_adv_start(
        own_addr_type,
        NULL,
        BLE_HS_FOREVER,
        &adv_params,
        ble_spp_server_gap_event,
        NULL
    );
}

/**
 * NimBLE GAP 이벤트 콜백 핸들러 (연결 상태 감시 + [추가] 비콘 스캔 처리)
 */
static int ble_spp_server_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_gap_conn_desc desc;
    struct ble_hs_adv_fields fields;
    int rc;


    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        MODLOG_DFLT(INFO, "connection %s; status=%d \n",
                    event->connect.status == 0 ? "established" : "failed", event->connect.status);
        ESP_LOGE(TAG, "BLE_GAP_EVENT_CONNECT");
        is_phone_connected = true;
        if (esp_timer_is_active(Mac_sending_timer)) {
            esp_timer_stop(Mac_sending_timer);
        }
        ESP_ERROR_CHECK(esp_timer_start_periodic(Mac_sending_timer,    3000000));

        
        current_conn_handle = event->connect.conn_handle;
        //MotionSetTimer(is_phone_connected); by.jeon 이 타이머를 왜 돌리는거죠?
        if (event->connect.status == 0) {
            if (ble_gap_conn_find(event->connect.conn_handle, &desc) == 0) {
                ble_spp_server_print_conn_desc(&desc);
            }
        } else {
            // 🔴 중요: 연결 실패(status != 0)일 때는 절대 바로 광고를 켜면 안 됩니다!
            MODLOG_DFLT(WARN, "Connection failed. Do not restart ADV immediately.\n");
        }
        return 0;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGE(TAG, "BLE_GAP_EVENT_DISCONNECT");
        MODLOG_DFLT(INFO, "disconnect; reason=%d \n", event->disconnect.reason);
        if (esp_timer_is_active(Mac_sending_timer)) {
            esp_timer_stop(Mac_sending_timer);
        }
        current_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        is_phone_connected = false;
        MotionSetTimer(is_phone_connected);
        if (event->disconnect.conn.conn_handle <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
            conn_handle_subs[event->disconnect.conn.conn_handle] = false;
        }
        ble_spp_server_advertise();
        return 0;

    case BLE_GAP_EVENT_SUBSCRIBE:
        MODLOG_DFLT(INFO, "subscribe event; conn_handle=%d attr_handle=%d \n",
                    event->subscribe.conn_handle, event->subscribe.attr_handle);
        if (event->subscribe.conn_handle <= CONFIG_BT_NIMBLE_MAX_CONNECTIONS) {
            conn_handle_subs[event->subscribe.conn_handle] = true;
        }
        return 0;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        ble_spp_server_advertise();
        return 0;
    case BLE_GAP_EVENT_CONN_UPDATE:
            MODLOG_DFLT(INFO, "Connection updated; status=%d\n", event->conn_update.status);
            
            // 현재 커넥션 핸들의 세부 정보를 가져옵니다.
            if (ble_gap_conn_find(event->conn_update.conn_handle, &desc) == 0) {
                MODLOG_DFLT(INFO, "--- BLE Conn Parameters ---\n");
                MODLOG_DFLT(INFO, "Conn Interval: %d (%.2f ms)\n", desc.conn_itvl, desc.conn_itvl * 1.25);
                MODLOG_DFLT(INFO, "Slave Latency: %d\n", desc.conn_latency);
                MODLOG_DFLT(INFO, "Supervision Timeout: %d (%d ms)\n", desc.supervision_timeout, desc.supervision_timeout * 10);
                
                // 💡 NimBLE 버전에 따라 desc 구조체 내부에 sec_state나 다른 멤버로 PHY가 찍히기도 하지만,
                // 가장 확실한 방법은 연결 직후 nRF52나 폰이 2Mbps를 지원하는지 '조회'하는 기능을 넣는 것입니다.
            }
            return 0;
    // 🔴 [추가] 주변에서 비콘 패킷을 가로챘을 때 일어나는 이벤트
    case BLE_GAP_EVENT_DISC:
    {
        // 주변 기기 정보를 통합 저장할 저장소 (최대 20개)
        typedef struct {
            uint8_t addr[6];
            char name[32];
            int8_t rssi;
        } dev_info_t;
        app_config_t* app_config = get_app_config();
        static dev_info_t dev_list[20];
        static int dev_count = 0;

       rc = ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (rc != 0) {
            return 0;
        }

        // -----------------------------------------------------------------
        // 1. ⚡️ [새로운 필터] RSSI 조건 검사 (간혹 부호가 헷갈릴 수 있으니 주의)
        //    * 수신 감도가 -55 dBm 이하(예: -56, -60, -70 dBm처럼 멀리 있는 기기)인 경우만 통과
        //    * 만약 -55보다 신호가 쌘 것(-50, -40 dBm 등)을 원하신 거라면 `>`로 부호를 바꿔주세요.
        // -----------------------------------------------------------------
        if (event->disc.rssi < app_config->gate_way_rssi_th) {
            return 0; // -55보다 큰 신호는 여기서 즉시 차단
        }

        // -----------------------------------------------------------------
        // 2. ⚡️ 임시 이름 확인 및 캐시 검색을 위한 준비
        //    * BLE 스캔 패킷 특성상 이름이 항상 들어오지 않으므로, 
        //      먼저 현재 패킷에 이름이 있거나 혹은 기존 캐시에 등록되어 있는지 찾아야 합니다.
        // -----------------------------------------------------------------
        int idx = -1;
        for (int i = 0; i < dev_count; i++) {
            if (memcmp(dev_list[i].addr, event->disc.addr.val, 6) == 0) {
                idx = i;
                break;
            }
        }

        // 현재 패킷에서 이름을 추출해 봅니다.
        char current_packet_name[32] = "";
        if (fields.name != NULL && fields.name_len > 0) {
            int len = (fields.name_len > 31) ? 31 : fields.name_len;
            memcpy(current_packet_name, fields.name, len);
            current_packet_name[len] = '\0';
        } else if (idx != -1) {
            // 이번 패킷에 이름은 없지만 기존 캐시 리스트에 저장된 이름이 있다면 가져옵니다.
            strcpy(current_packet_name, dev_list[idx].name);
        }
        // -----------------------------------------------------------------
        // 3. ⚡️ Service UUID 16-bit 검사 (0x1234 인지 확인)
        // -----------------------------------------------------------------
        if (fields.svc_data_uuid16 == NULL || fields.svc_data_uuid16_len <= 0) {
            return 0; // 서비스 데이터가 아예 없으면 차단
        }

        uint16_t svc_uuid = (fields.svc_data_uuid16[1] << 8) | fields.svc_data_uuid16[0];
        if (svc_uuid != 0x1234) {
            return 0; // 우리가 찾는 0x1234 서비스가 아니면 차단
        }
        // -----------------------------------------------------------------
        // 4. 🎉 모든 방어벽 통과! 이제야 안심하고 `dev_list`에 저장/업데이트 진행
        // -----------------------------------------------------------------
        if (idx == -1) {
            // 기존 리스트에 없던 신규 기기라면 새로 빈 공간 확보
            if (dev_count < 20) {
                idx = dev_count++;
            } else {
                static int rolling_idx = 0;
                idx = rolling_idx;
                rolling_idx = (rolling_idx + 1) % 20;
            }
            memset(&dev_list[idx], 0, sizeof(dev_info_t));
            memcpy(dev_list[idx].addr, event->disc.addr.val, 6);
        }

        // 최신 이름과 RSSI 데이터 업데이트 저장
        strcpy(dev_list[idx].name, current_packet_name);
        dev_list[idx].rssi = event->disc.rssi;


        // -----------------------------------------------------------------
        // 5. 🎯 최종 결과 콘솔 출력 (페이로드 파싱)
        // -----------------------------------------------------------------
        const uint8_t *actual_data = fields.svc_data_uuid16 + 2;
        int actual_data_len = fields.svc_data_uuid16_len - 2;

        printf("\n🎯 [Wave_Tracker] 조건 만족 기기 dev_list 저장 완료 🎯\n");
        printf("MAC  : %02X:%02X:%02X:%02X:%02X:%02X (%d dBm)\n",
               event->disc.addr.val[5], event->disc.addr.val[4], event->disc.addr.val[3],
               event->disc.addr.val[2], event->disc.addr.val[1], event->disc.addr.val[0],
               dev_list[idx].rssi);

        if (actual_data_len > 0) {
            char str_dump_buf[64] = {0}; 
            int buf_idx = 0;

            // 2. 원본 데이터 길이만큼 돌면서 필터링 후 버퍼에 적재
            for (int i = 0; i < actual_data_len; i++) {
                // ⚠️ 64바이트 버퍼가 가득 차는 것을 방지 (마지막 널 문자를 위해 62번 인덱스까지만 저장)
                if (buf_idx >= (sizeof(str_dump_buf) - 1)) {
                    break; 
                }

                char c = actual_data[i];
                
                // 출력 가능한 문자면 그대로 넣고, 아니면 점('.')으로 넣기
                str_dump_buf[buf_idx] = (c >= 32 && c <= 126) ? c : '.';
                buf_idx++;
            }

            // 3. 🌟 C언어 문자열의 완성: 맨 끝에 널 문자 강제 삽입 (안전장치)
            str_dump_buf[buf_idx] = '\0';
            Tracker_In_ID(str_dump_buf);
            // 4. 결과 출력 테스트
            printf("STR  : %s\n", str_dump_buf);
        } else {
            printf("HEX  : 서비스 데이터 페이로드가 비어있음\n");
        }
        printf("-------------------------------------\n");

        return 0;
    }

    default:
        return 0;
    }
}

static void ble_spp_server_on_reset(int reason)
{
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void ble_spp_server_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    assert(rc == 0);

    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        MODLOG_DFLT(ERROR, "error determining address type; rc=%d\n", rc);
        return;
    }

    // 내 신호 전송 시작
    ble_spp_server_advertise();

    // 🔴 [추가] 동기화 완료 후 비콘 스캔 엔진 가동
    ble_spp_client_scan();
}

void ble_spp_server_host_task(void *param)
{
    MODLOG_DFLT(INFO, "BLE Host Task Started\n");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/**
 * GATT 캐릭터리스틱 처리 (중괄호 블록을 넣어 컴파일 에러 완전 수정)
 */
static int ble_svc_gatt_handler(uint16_t conn_handle, uint16_t attr_handle, struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_WRITE_CHR: 
    { // 🟢 중괄호 추가로 switch-unreachable 변수선언 문제 해결
        uint16_t data_len = OS_MBUF_PKTLEN(ctxt->om);
        int8_t rssi = 0;
        // 현재 연결 핸들(conn_handle)의 RSSI를 조회합니다.

        struct ble_gap_conn_desc desc;
        uint8_t peer_mac[6] = {0};

        if (ble_gap_conn_find(conn_handle, &desc) == 0) {
            // desc.peer_id_addr.val 에 6바이트 MAC 주소가 들어있습니다.
            memcpy(peer_mac, desc.peer_id_addr.val, 6);
            
            // NimBLE의 MAC 주소 배열은 역순(Little-Endian)으로 들어올 수 있으므로
            // 대문자 16진수 문자열로 출력하여 확인해봅니다.
            ESP_LOGI("BLE_RX", "Connected Peer MAC: %02X:%02X:%02X:%02X:%02X:%02X",
                     peer_mac[5], peer_mac[4], peer_mac[3], 
                     peer_mac[2], peer_mac[1], peer_mac[0]);
        } else {
            ESP_LOGW("BLE_RX", "Failed to find connection info for handle: %d", conn_handle);
        }


        int rc = ble_gap_conn_rssi(conn_handle, &rssi);
        if (rc == 0) {
            printf("BLE Write Received - Data Len: %d, RSSI: %d dBm\n", data_len, rssi);
        } else {
            printf("BLE Write Received - Data Len: %d, (Failed to get RSSI, rc=%d)\n", data_len, rc);
        }     
        if (data_len > 0 && ble_rx_queue != NULL) {
            // 1. 구조체 변수를 스택에 임시 선언
            ble_data_msg_t msg;

            // 2. 실제 복사할 데이터 길이 계산 및 구조체 입력
            msg.len = data_len; 
            memcpy(msg.mac,peer_mac,sizeof(msg.mac));
            // 3. ★ 매우 중요: 힙(Heap) 영역에 실제 데이터를 담을 공간 할당!
            msg.data = (uint8_t *)malloc(msg.len);
            if (msg.data == NULL) {
                ESP_LOGE("BLE_RX", "Failed to allocate memory for RX data!");
                return 0; // 메모리가 부족할 때 NimBLE에 에러 반환
            }

            // 4. mbuf 내부의 이진 데이터를 방금 할당한 플랫 버퍼(msg.data)로 복사
            uint16_t copied_len = 0;
            ble_hs_mbuf_to_flat(ctxt->om, msg.data, msg.len, &copied_len);

            // 5. 큐로 구조체 메시지 송신 (구조체 내부의 '포인터 주소값'만 복사되어 전달됩니다)
            if (xQueueSend(ble_rx_queue, &msg, 0) != pdPASS) {
                ESP_LOGW("BLE_RX", "Queue full! Dropping packet and freeing memory.");
                // 큐가 꽉 차서 전송에 실패했다면, 메모리가 공중에 붕 뜨지 않게 여기서 즉시 해제해 주어야 합니다!
                free(msg.data); 
            }
        }
        break;
    } // 🟢 블록 마감

    default:
        break;
    }
    return 0;
}
// 2. GATT 정의부 수정
static const struct ble_gatt_svc_def new_ble_svc_gatt_defs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_svc_uuid.u, // 캐스팅 대신 구조체 주소 전달
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &nus_rx_uuid.u,
                .access_cb = ble_svc_gatt_handler,
                // 시리얼 통신 특성상 Write 응답 없는 편이 속도와 안정성에 좋습니다.
                .flags = BLE_GATT_CHR_F_WRITE_NO_RSP |BLE_GATT_CHR_F_WRITE, 
            },
            {
                .uuid = &nus_tx_uuid.u,
                .access_cb = ble_svc_gatt_handler,
                .val_handle = &ble_spp_svc_gatt_notify_val_handle, 
                .flags = BLE_GATT_CHR_F_NOTIFY ,
            },
            { 0 } 
        },
    },
    { 0 }, 
};

static void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];
    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n", ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf), ctxt->svc.handle);
        break;
    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with def_handle=%d val_handle=%d\n", ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf), ctxt->chr.def_handle, ctxt->chr.val_handle);
        break;
    default:
        break;
    }
}

int gatt_svr_init(void)
{
    ble_svc_gap_init();
    ble_svc_gatt_init();

    int rc = ble_gatts_count_cfg(new_ble_svc_gatt_defs);
    if (rc != 0) return rc;

    rc = ble_gatts_add_svcs(new_ble_svc_gatt_defs);
    if (rc != 0) return rc;

    return 0;
}

void ble_server_send_notify(uint16_t conn_handle, uint8_t *data, uint16_t len)
{
    struct os_mbuf *om;
    om = ble_hs_mbuf_from_flat(data, len);
    if (om != NULL) {
        esp_err_t err = ble_gatts_notify_custom(conn_handle, ble_spp_svc_gatt_notify_val_handle, om);

            if (err != ESP_OK) {
                // [4] 에러 분기 처리
                switch (err) {
                    case ESP_ERR_INVALID_STATE:
                        // 주로 상대방 기기(스마트폰 등)가 Notification을 On(구독)하지 않았을 때 발생
                        ESP_LOGW(TAG, "Notify failed: Client has not enabled notifications (CCCD not registered).");
                        break;

                    case ESP_ERR_NO_MEM:
                        // BLE 링버퍼나 패킷 큐가 가득 찼을 때 (너무 빠르게 연속으로 보낼 때 발생)
                        ESP_LOGE(TAG, "Notify failed: Out of memory / BLE TX Buffer Congested! Slow down the transfer rate.");
                        // 팁: 여기서 조금 대기(vTaskDelay)했다가 재시도(Retry)하는 로직을 추가할 수도 있습니다.
                        break;

                    case ESP_ERR_INVALID_ARG:
                        ESP_LOGE(TAG, "Notify failed: Invalid parameter (check handle or data length).");
                        break;

                    default:
                        // 정의되지 않은 기타 시스템 에러
                        ESP_LOGE(TAG, "Notify failed with system error code: 0x%X (%s)", err, esp_err_to_name(err));
                        break;
                }
                return;
            }

            // 성공 시 가볍게 디버깅 로그 (필요할 때만 켭니다)
            ESP_LOGD(TAG, "Notification sent successfully. Len: %d", len);
            //return ESP_OK;
            return;
    }
    ESP_LOGD(TAG, "om is NULL");
}
/**
 * @brief 스마트폰으로 보낼 데이터를 BLE 송신 큐에 집어넣는 함수
 * * @param data 보낼 데이터의 바이트 배열 포인터
 * @param len  보낼 데이터의 길이 (바이트 단위)
 * @return true 큐에 성공적으로 진입함, false 큐가 가득 찼거나 비활성화 상태
 */
bool ble_send_data_to_queue(const uint8_t *data, uint16_t len)
{
    // 1. 안전 장치: 데이터가 없거나 길이가 0인 경우 차단
    if (data == NULL || len == 0) {
        return false;
    }

    // 2. 안전 장치: 큐가 생성되지 않은 상태라면 차단
    if (ble_tx_queue == NULL) {
        printf("[BLE TX FUNC] 에러: ble_tx_queue가 초기화되지 않았습니다.\n");
        return false;
    }

    ble_data_msg_t msg;

    // 3. 방어 코드: 구조체 배열 크기(512바이트)를 넘지 않도록 길이 제한
    msg.len = len;
    

    // 3. ★ 매우 중요: 힙(Heap) 영역에 실제 데이터를 담을 공간 할당!
    msg.data = (uint8_t *)malloc(msg.len);
    if (msg.data == NULL) {
        ESP_LOGE("BLE_RX", "Failed to allocate memory for RX data!");
        return false; // 메모리가 부족할 때 NimBLE에 에러 반환
    }
    // 4. 데이터 실제 복사
    memcpy(msg.data, data, msg.len);

    // 5. 큐로 전송 (포트 대기 시간은 0으로 설정하여 현재 태스크가 멈추지 않게 함)
    if (xQueueSend(ble_tx_queue, &msg, 0) != pdTRUE) {
        ESP_LOGW("BLE_RX", "Queue full! Dropping packet and freeing memory.");
        // 큐가 꽉 차서 전송에 실패했다면, 메모리가 공중에 붕 뜨지 않게 여기서 즉시 해제해 주어야 합니다!
        free(msg.data); 
        return false;
    }
    return true;
}
static void ble_rx_processing_task(void *pvParameters)
{
    ble_data_msg_t msg;
    MODLOG_DFLT(INFO, "사용자 데이터 처리 태스크 시작\n");

    while (1) {
        if (xQueueReceive(ble_rx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            BLE_Receive_data(msg.mac, msg.data, msg.len);
            free(msg.data);   
        }
    }
}
static void ble_tx_processing_task(void *pvParameters)
{
    ble_data_msg_t msg;
    MODLOG_DFLT(INFO, "BLE 송신 전송 전담 태스크 시작\n");

    while (1) {
        // 내가 폰으로 보낼 데이터가 큐에 들어올 때까지 대기
        if (xQueueReceive(ble_tx_queue, &msg, portMAX_DELAY) == pdTRUE) {
            if (is_phone_connected && current_conn_handle != BLE_HS_CONN_HANDLE_NONE) {

                uint16_t offset = 0;
                while (offset < msg.len) { // MTU 길이만큼 데이터를 나눠서 보낸다.
                    // 이번 턴에 보낼 길이: 남은 길이와 MTU 최대치 중 작은 값
                    uint16_t send_len = (msg.len - offset > g_ble_max_payload) ? g_ble_max_payload : (msg.len - offset);
                    
                    // msg.data의 offset 위치부터 send_len 만큼 잘라서 쏘기
                    ble_server_send_notify(current_conn_handle, &msg.data[offset], send_len);
                    printf("[TX 태스크] %d 바이트 중 %d 바이트 쪼개서 전송 완료 (offset: %d)\n", msg.len, send_len, offset);
                    
                    offset += send_len;
                    
                    // 연속 전송 시 BLE 컨트롤러 큐 오버플로우 방지 (필수)
                    vTaskDelay(pdMS_TO_TICKS(15));
                }
                printf("[TX 태스크] 스마트폰으로 %d 바이트 Notify 전송 완료\n", msg.len);
            } else {
                printf("[TX 태스크] 경고: 스마트폰이 연결되어 있지 않아 전송 취소\n");
            }
            free(msg.data);   
        }
    }
}



static void mac_send_timer_callback(void* arg)
{
    esp_timer_stop(Mac_sending_timer);
    uint8_t mac[6];
    //uint8_t Str[40];
    char Str[150];
    esp_read_mac(mac,ESP_MAC_WIFI_STA);
    // sprintf((char*)Str, "Wifi MAC %02X%02X%02X%02X%02X%02X", 
    // mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    // [by.jeon] ble 연결 직후 meta 데이터를 보내야 한다.
    snprintf(Str, sizeof(Str), 
             "{\"event_type\":\"meta\",\"data\":{\"serial\":\"%s-%02X%02X%02X%02X%02X%02X\",\"model\":\"%s\",\"hw_rev\":\"%s\",\"fw\":\"%s\"}}", 
             CONFIG_DEVICE_PREFIX,                               // W100
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],     // MAC Address
             CONFIG_DEVICE_TYPE,                                 // w100
             CONFIG_HW_REV,                                      // r1.0
             CONFIG_FW_VERSION);                                 // v1.0.0
    printf("send %s ", Str);
    ble_send_data_to_queue((const uint8_t*)Str, strlen((const char*)Str));
}

void motion_msg_send(uint8_t cmd,uint8_t sub_cmd)
{
    Motion_Packet_t Motion_Packet;

    memset(&Motion_Packet,0,sizeof(Motion_Packet));
        
    switch(cmd)
    {
        case MOTION_START_REQUEST:
            Motion_Packet.event_code = cmd;
            Motion_Packet.motion_res.req_type = sub_cmd;
        break;
        case HEALTH_DATA_REQUEST:
            Motion_Packet.event_code = cmd;
            Motion_Packet.health_data_req.req_type = sub_cmd;
        break;    
        case MOTION_DATA_ACK:
            Motion_Packet.event_code = cmd;
            Motion_Packet.motion_ack.ack_seq_no = sub_cmd;
        break;
        case OTA_MODE_REQUEST:
            Motion_Packet.event_code = cmd;
            Motion_Packet.ota_req.cmd_type = sub_cmd;
        break;   

        default : 
        return;            
    }



    ble_send_data_to_queue((uint8_t*)&Motion_Packet,sizeof(Motion_Packet));
    // 설정값이 0보다 클 때만 타이머 실행 (안전성 강화)
}
#include "esp_bt.h" // 필요한 헤더 포함
void ble_task_init(void)
{
    esp_err_t ret;
    ret = nimble_port_init();
    if (ret != ESP_OK) {
        MODLOG_DFLT(ERROR, "Failed to init nimble %d \n", ret);
        return;
    }

    for (int i = 0; i < CONFIG_BT_NIMBLE_MAX_CONNECTIONS; i++) {
        conn_handle_subs[i] = false;
    }

    ble_hs_cfg.reset_cb = ble_spp_server_on_reset;
    ble_hs_cfg.sync_cb = ble_spp_server_on_sync;
    ble_hs_cfg.gatts_register_cb = gatt_svr_register_cb;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    esp_err_t err = esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_DEFAULT, ESP_PWR_LVL_P15);
    
    if (err == ESP_OK) {
        printf("BLE TX Power set successfully!\n");
    } else {
        printf("Failed to set BLE TX Power: %d\n", err);
    }
    #ifndef CONFIG_EXAMPLE_IO_TYPE
    #define CONFIG_EXAMPLE_IO_TYPE 3 
    #endif
    ble_hs_cfg.sm_io_cap = CONFIG_EXAMPLE_IO_TYPE;

    assert(gatt_svr_init() == 0);
    assert(ble_svc_gap_device_name_set(DEVICE_NAME) == 0);

    ble_store_config_init();


    const esp_timer_create_args_t mac_sending_timer_args = {
        .callback = &mac_send_timer_callback,
        .name = "mac_sending_timer"
    };
    ESP_ERROR_CHECK(esp_timer_create(&mac_sending_timer_args, &Mac_sending_timer));


    ble_rx_queue = xQueueCreate(10, sizeof(ble_data_msg_t));
    ble_tx_queue = xQueueCreate(10, sizeof(ble_data_msg_t));

    // xTaskCreate 대신 xTaskCreatePinnedToCore를 사용합니다.
    if (xTaskCreatePinnedToCore(
            ble_rx_processing_task,                  // 태스크 함수
            "ble_rx_task",                // 태스크 이름
            BLE_TRX_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 3,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
                ESP_LOGE(TAG, "Error creating ble_rx_task on Core 1");
    }

    if (xTaskCreatePinnedToCore(
            ble_tx_processing_task,                  // 태스크 함수
            "ble_tx_task",                // 태스크 이름
            BLE_TRX_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 3,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
              ESP_LOGE(TAG, "Error creating ble_tx_task on Core 1");
    }

    nimble_port_freertos_init(ble_spp_server_host_task);
}