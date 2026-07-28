#include "ble_tracker_id.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
// 특정 장치의 타이머 시작
#include <string.h>
#include "opmode_task.h"
#include "esp_log.h" // 👈 ESP_LOGE를 위해 추가
#include "app_config_flash.h"
#include "ble_task.h"
#include "ble_parse.h"
TaskHandle_t xTrackerCaptureHandle = NULL;
static const char *TAG = __FILE__;
void Tracker_Device_disable(int i);



#define TRACKER_TASK_STACK_SIZE (configMINIMAL_STACK_SIZE * 2)
#define TRACKER_DEVICE_MAX 20
Tracker_Device_t* Tracker_Device[TRACKER_DEVICE_MAX];
Tracker_Device_t Tracker_UNKNOWN = 
{
    .Device_ID = "UNKNOWN",
};
Tracker_Device_t* Get_Tracker_Device(uint8_t* addr)
{
    int idx = -1;
    for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
        if(Tracker_Device[i] != NULL)
        {
            if (memcmp(Tracker_Device[i]->dev_info.addr, addr, 6) == 0) {
                idx = i;
                break;
            }
        }
    }
    if(idx == -1)
        return NULL;

    return Tracker_Device[idx];
}
// 1. 단일 디바이스 구조체 정보를 출력하는 헬퍼 함수
void dump_tracker_device_info(const char* label, const Tracker_Device_t* dev)
{
    if (dev == NULL) {
        printf("%s: [ NULL ]\n", label);
        return;
    }
    
    printf("%s\n", label);
  
    printf(" Device_ID       : %s\n", dev->Device_ID);

    printf(" total_Device_Time : %ld\n", dev->total_Device_Time);
    printf(" Device_Time       : %ld\n", dev->Device_Time);
    printf(" Disable_Time      : %ld\n", dev->Disable_Time);
    printf(" diff_Time         : %ld\n", dev->diff_Time);
    printf(" Enable            : %ld\n", dev->Enable);
    printf(" Water_intake      : %ld\n", dev->Water_intake);

    printf(" ---- dev_info ----\n");

    printf(" addr             : ");
    for (int i = 0; i < 6; i++)
    {
        printf("%02X", dev->dev_info.addr[i]);
        if (i < 5)
            printf(":");
    }
    printf("\n");

    printf(" name             : %s\n", dev->dev_info.name);
    printf(" rssi             : %d\n", dev->dev_info.rssi);

    printf("-----------------------------\n");
}

// 2. 전체 Tracker_Device 배열 및 UNKNOWN 객체를 덤프하는 함수
void dump_tracker_all_devices(void)
{
    printf("\n==================== TRACKER DEVICE DUMP START ====================\n");
    
    // 2-1. Tracker_UNKNOWN 구조체 덤프
    dump_tracker_device_info("[ Tracker_UNKNOWN Object ]", &Tracker_UNKNOWN);
    printf("-------------------------------------------------------------------\n");

    // 2-2. Tracker_Device 포인터 배열 덤프
    printf("[ Tracker_Device Array (Max: %d) ]\n", TRACKER_DEVICE_MAX);
    
    int active_count = 0;
    for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
        if (Tracker_Device[i] != NULL) {
            char index_label[32];
            snprintf(index_label, sizeof(index_label), "  Index [%d]", i);
            dump_tracker_device_info(index_label, Tracker_Device[i]);
            active_count++;
        }
    }
    
    if (active_count == 0) {
        printf("  (배열이 비어있습니다. 등록된 디바이스가 없습니다.)\n");
    }

    printf("===================== TRACKER DEVICE DUMP END =====================\n\n");
}
void Tracker_waterintake_end(uint32_t Weight)
{
    for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
        if (Tracker_Device[i] != NULL) {
            if(Tracker_Device[i]->Enable)
                Tracker_Device_disable(i);
        }
    }
    // 2. 전체 diff_Time의 총합을 구합니다.
    uint32_t total_time_sum = 0;
    for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
        if (Tracker_Device[i] != NULL) {
            total_time_sum += Tracker_Device[i]->diff_Time;
        }
    }
    // 예외 처리: 아무도 마시지 않았다면 리턴
    if (total_time_sum == 0) 
    {
        Tracker_UNKNOWN.Water_intake += Weight;
        return;
    }


    printf("\n================= 물 분배 정산 결과 =================\n");
    printf("총 마신 물 (Weight): %ld ml | 총 기여 시간: %ld ms\n", Weight, total_time_sum);
    printf("-----------------------------------------------------\n");

    // 3. 오직 diff_Time 비율로만 물을 쪼개고, 계산 끝난 diff_Time은 0으로 클리어합니다.
    for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
        if (Tracker_Device[i] != NULL && Tracker_Device[i]->diff_Time > 0) {
            
            // 비율 계산 (정수 오차 방지)
            uint32_t allocated_water = ((uint64_t)Tracker_Device[i]->diff_Time * Weight) / total_time_sum;
            
            // [필요시 각 구조체 누적용 변수에 더해주기]
            // Tracker_Device[i]->total_Weight += allocated_water;

            printf("[%s] 마신 시간(diff): %ld ms | 분배된 물: %ld ml\n", 
                   Tracker_Device[i]->Device_ID, Tracker_Device[i]->diff_Time, allocated_water);
            Tracker_Device[i]->Water_intake += allocated_water;
            // 사용이 끝났으므로 다음 턴을 위해 클리어
            Tracker_Device[i]->diff_Time = 0;
        }
    }
    printf("=====================================================\n");
}
void Tracker_Device_disable(int i)
{
    if(i >= TRACKER_DEVICE_MAX || i < 0) return;

    Tracker_Device[i]->total_Device_Time += Tracker_Device[i]->Device_Time;
    Tracker_Device[i]->diff_Time = Tracker_Device[i]->Device_Time;
    Tracker_Device[i]->Enable = 0;
    Tracker_Device[i]->Device_Time = 0;
}

bool Tracker_device_time_add(int i)
{
    bool ret = false;

    if (Tracker_Device[i] != NULL) 
    {
        if (Tracker_Device[i]->Enable) {

            Tracker_Device[i]->Device_Time += 100;
            //printf("[%s] total time %ld \n", Tracker_Device[i]->Device_ID, Tracker_Device[i]->Device_Time);
            ret = true;
        }
    }
    return ret;
}   

void Tracker_In_ID(dev_info_t* dev_info, char* Tracker_ID)
{
    int target_index = -1;
    Tracker_Device_t* Tracker_target = Get_Tracker_Device(dev_info->addr);


    printf(" addr             : ");
    for (int i = 0; i < 6; i++)
    {
    printf("%02X", dev_info->addr[i]);
    if (i < 5)
        printf(":");
    }
    printf("\n");

    printf(" name             : %s\n", dev_info->name);
    printf(" rssi             : %d\n", dev_info->rssi);
    if(Tracker_target != NULL)
    {
        Tracker_target->Enable = 1;
        Tracker_target->Disable_Time = 0;
        memcpy(&Tracker_target->dev_info,dev_info,sizeof(dev_info_t));
        return;
    }
    // 🌟 배열을 처음부터 끝까지 스캔하며 중복 검사 및 빈자리 탐색
    for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
        // 1. 자리가 채워져 있는 경우 -> ID 중복 검사 수행
        if (Tracker_Device[i] == NULL) {
            target_index = i; // 최초 1회만 빈자리 저장 (이후 루프는 중복 검사를 위해 계속 돎)
            break;
        }
    }

    // 3. 🚨 루프를 끝까지 돌았는데도 빈자리가 없다면 (target_index가 여전히 -1이면) return
    if (target_index == -1) {
        printf("[에러] 장치 생성 실패: 저장 공간이 가득 찼습니다! (MAX: %d)\n", TRACKER_DEVICE_MAX);
        return; 
    }

    Tracker_Device_t* p_dev = (Tracker_Device_t*)malloc(sizeof(Tracker_Device_t));
    if (p_dev == NULL) return;

    // 🛠️ 2. [수정] 데이터 명확하게 초기화 (쓰레기 값 제거)
    memset(p_dev, 0, sizeof(Tracker_Device_t)); // 전체 0으로 초기화

    memcpy(&p_dev->dev_info,dev_info,sizeof(dev_info_t));
    // 2. 데이터 초기화 (시간은 0ms부터 시작)
    strncpy(p_dev->Device_ID, Tracker_ID, sizeof(p_dev->Device_ID) - 1);
    p_dev->Enable = 1;
    // 4. 전역 배열에 등록
    Tracker_Device[target_index] = p_dev;
    printf("장치 [%s] 생성 완료 \n", Tracker_ID);
}







static bool Motion_Send_enable = false;
static int Motion_Send_tick = 50;



void MotionSetTimer(bool status)
{
    if(Motion_Send_enable != status)
    {
        Motion_Send_enable = status;
    }
    if(Motion_Send_enable)
    {
        Motion_Send_tick = 50;
    }
}
bool GetTracker_Id_active(void)
{
    for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
        if(Tracker_Device[i] != NULL)
        {
            if(Tracker_Device[i]->Enable)
            {
                return true;
            }
        }
    }   
    return false;
}
/**
 * @brief 100ms 주기로 실행될 Tracker Capture 태스크 함수
 */
void vTrackerCaptureTask(void *pvParameters)
{
    // 100ms 주기를 틱(Tick) 단위로 변환
    TickType_t xDelay = pdMS_TO_TICKS(100);
    TickType_t xLastWakeTime;

    // 현재 틱 카운트로 초기화 (정밀한 주기 유지를 위함)
    xLastWakeTime = xTaskGetTickCount();

    printf("[태스크] tracker_capture 태스크가 시작되었습니다. (주기: 100ms)\n");
    app_config_t* app_config = get_app_config();
    for (;;)
    {
        /*-----------------------------------------------------------------*/
        /* 🎯 여기에 100ms마다 실행할 캡처 및 처리 로직을 작성하세요.       */
        /* 예: 센서 데이터 읽기, Tracker 상태 업데이트 등                  */
        /*-----------------------------------------------------------------*/
        // printf("[캡처] 데이터를 캡처하는 중...\n"); 
        if(Motion_Send_enable)
        {
            Motion_Send_tick--;
            if(Motion_Send_tick <= 0)
            {
                Motion_Send_tick = (app_config->motion_data_time) * 10;
                motion_msg_send(MOTION_START_REQUEST,1);
            }
        }
        for (int i = 0; i < TRACKER_DEVICE_MAX; i++) {
            if(Time_ratio_state() == SMART_RUN_STABLE)
            {      
                Tracker_device_time_add(i);
            }
            if(Tracker_Device[i] != NULL)
            {
                if(Tracker_Device[i]->Enable)
                {
                    Tracker_Device[i]->Disable_Time += 100;
                    if(Tracker_Device[i]->Disable_Time >= 5000)
                    {
                        Tracker_Device_disable(i);
                    }  
                }
            }

        }   

        
        // 💡 vTaskDelayUntil은 이전 깨어난 시간 기준으로 정확히 100ms 뒤에 깨어나도록 보장합니다.
        // (vTaskDelay보다 주기성을 유지하는 데 훨씬 유리합니다)
        vTaskDelayUntil(&xLastWakeTime, xDelay);
    }
}

/**
 * @brief 초기화 루틴이나 main 함수에서 호출하여 태스크를 생성하는 함수
 */
void Create_Tracker_Capture_Task(void)
{
    if (xTaskCreatePinnedToCore(
            vTrackerCaptureTask,                  // 태스크 함수
            "tracker_capture",                // 태스크 이름
            TRACKER_TASK_STACK_SIZE,       // 스택 크기
            NULL,        // 파라미터
            tskIDLE_PRIORITY + 1,      // 우선순위
            NULL,                  // 태스크 핸들
            1                          // ⭐ 코어 ID (1번 코어 = APP_CPU)
        ) != pdPASS) {                 // pdTRUE 대신 pdPASS를 쓰는 것이 FreeRTOS 관례입니다.
              ESP_LOGE(TAG, "Error creating ble_tx_task on Core 1");
    }


}








