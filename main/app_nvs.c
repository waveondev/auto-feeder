#include "app_nvs.h"
#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = __FILE__;






// [쓰기 함수] 
// len 매개변수는 nvs_set_str 내부에서 자동으로 길이를 계산하므로 사실상 안 써도 무방합니다.
void write_nvs_memory(const char* name, const char* key, const char* data)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[%s] NVS 열기 실패 (%s)", name, esp_err_to_name(err));
        return;
    }

    // 매개변수 key를 그대로 사용
    err = nvs_set_str(my_handle, key, data);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[%s -> %s] 저장 성공: %s", name, key, data);
    } else {
        ESP_LOGE(TAG, "[%s -> %s] 저장 실패 (%s)", name, key, esp_err_to_name(err));
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) ESP_LOGE(TAG, "커밋 실패!");

    nvs_close(my_handle);
}

// [읽기 함수] 
// 외부에서 데이터를 담아갈 빈 그릇(out_data)과 그 그릇의 최대 크기(max_len)를 받도록 수정
esp_err_t read_nvs_memory(const char* name, const char* key, char* out_data, uint16_t max_len)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(name, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[%s] NVS 열기 실패 (%s)", name, esp_err_to_name(err));
        return err;
    }

    // 1. 크기 확인 단계 (매개변수 key 사용)
    size_t required_size = 0;
    err = nvs_get_str(my_handle, key, NULL, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "[%s -> %s] 저장된 데이터가 없습니다.", name, key);
        nvs_close(my_handle);
        return err;
    }

    // 2. 실제 읽기 단계
    if (err == ESP_OK && required_size > 0) {
        // 우리가 외부에서 준비한 그릇(max_len)보다 NVS에 저장된 데이터가 더 크면 에러 처리 (안전장치)
        if (required_size > max_len) {
            ESP_LOGE(TAG, "버퍼 크기가 부족합니다. (필요: %d, 수용가능: %d)", required_size, max_len);
            nvs_close(my_handle);
            return ESP_ERR_INVALID_SIZE;
        }

        // 매개변수 key를 사용해 out_data 그릇에 직접 데이터를 받아옵니다.
        err = nvs_get_str(my_handle, key, out_data, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[%s -> %s] 읽기 성공: %s", name, key, out_data);
        } else {
            ESP_LOGE(TAG, "[%s -> %s] 읽기 실패 (%s)", name, key, esp_err_to_name(err));
        }
    }

    nvs_close(my_handle);
    return err;
}


// [정수형 쓰기 함수]

void write_nvs_int(const char* name, const char* key, int32_t value)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[%s] NVS 열기 실패 (%s)", name, esp_err_to_name(err));
        return;
    }

    // 💡 정수 저장 함수는 nvs_set_i32 를 사용합니다.
    err = nvs_set_i32(my_handle, key, value);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[%s -> %s] 정수 저장 성공: %ld", name, key, value);
    } else {
        ESP_LOGE(TAG, "[%s -> %s] 정수 저장 실패 (%s)", name, key, esp_err_to_name(err));
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) ESP_LOGE(TAG, "커밋 실패!");

    nvs_close(my_handle);
}

// [정수형 읽기 함수]
// 기본값을 매개변수(default_value)로 주면, 저장된 게 없을 때 안전하게 그 값을 리턴합니다.
int32_t read_nvs_int(const char* name, const char* key, int32_t default_value)
{
    nvs_handle_t my_handle;
    esp_err_t err;
    int32_t out_value = default_value; // 초기값은 기본값으로 세팅

    err = nvs_open(name, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[%s] NVS 열기 실패 (%s)", name, esp_err_to_name(err));
        return default_value; 
    }

    // 💡 정수 읽기 함수는 nvs_get_i32 를 사용하며, 크기 확인 필요 없이 바로 읽습니다!
    err = nvs_get_i32(my_handle, key, &out_value);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[%s -> %s] 정수 읽기 성공: %ld", name, key, out_value);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "[%s -> %s] 저장된 정수가 없어 기본값(%ld)을 사용합니다.", name, key, default_value);
    } else {
        ESP_LOGE(TAG, "[%s -> %s] 정수 읽기 실패 (%s)", name, key, esp_err_to_name(err));
    }

    nvs_close(my_handle);
    return out_value; // 읽어온 값(또는 기본값) 리턴
}



// [구조체/바이너리 쓰기 함수]
void write_nvs_blob(const char* name, const char* key, const void* blob_data, size_t length)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[%s] NVS 열기 실패 (%s)", name, esp_err_to_name(err));
        return;
    }

    // 💡 구조체 같은 바이너리는 nvs_set_blob 을 사용합니다.
    err = nvs_set_blob(my_handle, key, blob_data, length);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "[%s -> %s] 구조체 저장 성공 (%d 바이트)", name, key, length);
    } else {
        ESP_LOGE(TAG, "[%s -> %s] 구조체 저장 실패 (%s)", name, key, esp_err_to_name(err));
    }

    err = nvs_commit(my_handle);
    if (err != ESP_OK) ESP_LOGE(TAG, "커밋 실패!");

    nvs_close(my_handle);
}

// [구조체/바이너리 읽기 함수]
esp_err_t read_nvs_blob(const char* name, const char* key, void* out_blob, size_t max_length)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    err = nvs_open(name, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "[%s] NVS 열기 실패 (%s)", name, esp_err_to_name(err));
        return err;
    }

    // 1. 저장된 BLOB의 실제 크기 확인
    size_t required_size = 0;
    err = nvs_get_blob(my_handle, key, NULL, &required_size);

    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "[%s -> %s] 저장된 구조체 데이터가 없습니다.", name, key);
        nvs_close(my_handle);
        return err;
    }

    // 2. 실제 읽기 단계
    if (err == ESP_OK && required_size > 0) {
        // 내보낼 그릇보다 NVS 데이터가 크면 오버플로우 방지
        if (required_size > max_length) {
            ESP_LOGE(TAG, "구조체 버퍼 크기 부족 (필요: %d, 그릇: %d)", required_size, max_length);
            nvs_close(my_handle);
            return ESP_ERR_INVALID_SIZE;
        }

        // 실제 구조체 포인터에 데이터 복사
        err = nvs_get_blob(my_handle, key, out_blob, &required_size);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "[%s -> %s] 구조체 읽기 성공 (%d 바이트)", name, key, required_size);
        } else {
            ESP_LOGE(TAG, "[%s -> %s] 구조체 읽기 실패 (%s)", name, key, esp_err_to_name(err));
        }
    }

    nvs_close(my_handle);
    return err;
}

void delete_nvs_key(const char* name, const char* key)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 1. 수정해야 하므로 NVS_READWRITE 모드로 열기
    err = nvs_open(name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return;

    // 2. 🔥 특정 Key 삭제 함수 호출
    err = nvs_erase_key(my_handle, key);
    if (err == ESP_OK) {
        ESP_LOGI("NVS", "[%s] Key 삭제 성공", key);
    } else if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "[%s] 지우려고 보니 원래 없는 Key였습니다.", key);
    }

    // 3. 지웠다는 사실을 최종 커밋(플래시에 반영)하고 닫기
    nvs_commit(my_handle);
    nvs_close(my_handle);
}

void delete_nvs_namespace(const char* name)
{
    nvs_handle_t my_handle;
    esp_err_t err;

    // 1. NVS_READWRITE 모드로 열기
    err = nvs_open(name, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) return;

    // 2. 🔥 해당 Namespace 안의 모든 Key 일괄 삭제!
    err = nvs_erase_all(my_handle);
    if (err == ESP_OK) {
        ESP_LOGI("NVS", "[%s] 네임스페이스 전체 삭제 성공 (초기화 완료)", name);
    }

    // 3. 커밋 후 핸들 닫기
    nvs_commit(my_handle);
    nvs_close(my_handle);
}



