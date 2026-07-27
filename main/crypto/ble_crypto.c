#include "ble_crypto.h"
#include <string.h>
#include "esp_log.h"
#include "esp_random.h"
#include "mbedtls/ecdh.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/chachapoly.h"
#include "mbedtls/gcm.h"
#include "mbedtls/base64.h"

static const char *TAG = "BLE_CRYPTO";

// 런타임 중 유지될 세션 상태 구조체
typedef struct {
    mbedtls_ecdh_context ecdh;
    uint8_t session_key[32]; 
    bool is_encrypted;       
} ble_session_t;

// ble session 저장 전역 변수
static ble_session_t g_ble_session;

// ESP32 하드웨어 난수 생성기 연동
static int hw_rng(void *p_rng, unsigned char *buf, size_t len) {
    esp_fill_random(buf, len);
    return 0;
}

esp_err_t ble_crypto_init_and_get_pubkey(char *out_base64_pubkey, size_t max_len) {
    int ret;
    uint8_t tls_pubkey[35]; // 넉넉한 버퍼 (mbedTLS는 33바이트를 출력함)
    size_t olen = 0;

    memset(&g_ble_session, 0, sizeof(ble_session_t));
    mbedtls_ecdh_init(&g_ble_session.ecdh);

    // 1. Curve25519 셋업
    ret = mbedtls_ecdh_setup(&g_ble_session.ecdh, MBEDTLS_ECP_DP_CURVE25519);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to setup Curve25519 (ret=-0x%04X)", -ret);
        return ESP_FAIL;
    }

    // 2. 키쌍 생성 및 공개키 추출 (33바이트 기록됨)
    ret = mbedtls_ecdh_make_public(&g_ble_session.ecdh, &olen, tls_pubkey, sizeof(tls_pubkey), hw_rng, NULL); 
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to generate public key (ret=-0x%04X)", -ret);
        return ESP_FAIL;
    }

    // 3. mbedTLS의 TLS 포맷(길이 1바이트 + 원본 32바이트)에서 순수 32바이트만 발라내기
    uint8_t *raw_pubkey = tls_pubkey;
    if (olen == 33 && tls_pubkey[0] == 32) {
        raw_pubkey = tls_pubkey + 1; // 첫 1바이트(0x20) 스킵
    } else if (olen != 32) {
        ESP_LOGE(TAG, "Unexpected public key length: %d", olen);
        return ESP_FAIL;
    }

    // 4. 순수 32바이트만 Base64 인코딩하여 폰으로 전송 준비
    size_t b64_olen = 0;
    ret = mbedtls_base64_encode((unsigned char *)out_base64_pubkey, max_len, &b64_olen, raw_pubkey, 32);
    if (ret != 0) return ESP_FAIL;

    ESP_LOGI(TAG, "Device public key generated successfully.");
    return ESP_OK;
}

esp_err_t ble_crypto_compute_session_key(const char *app_base64_pubkey) {
    int ret;
    uint8_t app_raw_pubkey[32];
    size_t olen = 0;

    // 1. 앱의 공개키 Base64 디코딩 (순수 32바이트)
    ret = mbedtls_base64_decode(app_raw_pubkey, sizeof(app_raw_pubkey), &olen, (const unsigned char *)app_base64_pubkey, strlen(app_base64_pubkey));
    if (ret != 0 || olen != 32) {
        ESP_LOGE(TAG, "Invalid App public key format");
        return ESP_FAIL;
    }

    // 2. mbedTLS가 읽을 수 있도록 앞에 길이 1바이트(0x20)를 강제로 붙여서 33바이트로 둔갑
    uint8_t tls_app_pubkey[33];
    tls_app_pubkey[0] = 32; // X25519 길이 명시
    memcpy(tls_app_pubkey + 1, app_raw_pubkey, 32);

    // 3. 둔갑시킨 33바이트를 Context에 주입
    ret = mbedtls_ecdh_read_public(&g_ble_session.ecdh, tls_app_pubkey, sizeof(tls_app_pubkey));
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to read app public key (ret=-0x%04X)", -ret);
        return ESP_FAIL;
    }

    // 4. 공유 비밀(Shared Secret) S 계산
    uint8_t shared_secret[32];
    size_t secret_len = 0;
    ret = mbedtls_ecdh_calc_secret(&g_ble_session.ecdh, &secret_len, shared_secret, sizeof(shared_secret), hw_rng, NULL);
    if (ret != 0 || secret_len != 32) {
        ESP_LOGE(TAG, "Failed to calculate shared secret (ret=-0x%04X)", -ret);
        return ESP_FAIL;
    }

    // 5. HKDF-SHA256(S) 로 최종 256비트 AES 세션키 도출
    ret = mbedtls_hkdf(mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 
                       NULL, 0,               
                       shared_secret, secret_len,     
                       NULL, 0,               
                       g_ble_session.session_key, 32); 
    
    if (ret != 0) {
        ESP_LOGE(TAG, "HKDF key derivation failed (ret=-0x%04X)", -ret);
        return ESP_FAIL;
    }

    g_ble_session.is_encrypted = true; 
    
    // 핸드셰이크 개인키는 즉시 파기 (Forward Secrecy 방어)
    mbedtls_ecdh_free(&g_ble_session.ecdh); 
    
    ESP_LOGI(TAG, "ECDH Handshake successful. Session Key generated.");
    return ESP_OK;
}

esp_err_t ble_crypto_decrypt(const uint8_t *ciphertext, size_t cipher_len,
                             const uint8_t *iv, const uint8_t *tag,
                             uint8_t *out_plaintext, size_t *out_len) {
    if (!g_ble_session.is_encrypted) {
        ESP_LOGE(TAG, "Session is not encrypted yet");
        return ESP_ERR_INVALID_STATE;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, g_ble_session.session_key, 256); 
    if (ret != 0) goto exit;

    // AES-256-GCM 복호화 및 인증
    ret = mbedtls_gcm_auth_decrypt(&gcm, cipher_len, 
                                   iv, 12,          
                                   NULL, 0,         
                                   tag, 16,         
                                   ciphertext, out_plaintext);
    
    if (ret == 0) {
        *out_len = cipher_len;
    } else {
        ESP_LOGE(TAG, "Decryption or Authentication failed (ret=-0x%04X)", -ret);
    }

exit:
    mbedtls_gcm_free(&gcm);
    return (ret == 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t ble_crypto_encrypt(const uint8_t *plaintext, size_t plain_len,
                             uint8_t *out_iv, uint8_t *out_tag, uint8_t *out_ciphertext) {
    if (!g_ble_session.is_encrypted) {
        ESP_LOGE(TAG, "Session is not encrypted yet");
        return ESP_ERR_INVALID_STATE;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);

    // IV(Nonce) 12바이트 랜덤 생성
    esp_fill_random(out_iv, 12); 

    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, g_ble_session.session_key, 256);
    if (ret != 0) goto exit;

    // AES-256-GCM 암호화 연산
    ret = mbedtls_gcm_crypt_and_tag(&gcm, MBEDTLS_GCM_ENCRYPT, plain_len, 
                                    out_iv, 12, NULL, 0, 
                                    plaintext, out_ciphertext, 16, out_tag);
    if (ret != 0) {
        ESP_LOGE(TAG, "Encryption failed (ret=-0x%04X)", -ret);
    }

exit:
    mbedtls_gcm_free(&gcm);
    return (ret == 0) ? ESP_OK : ESP_FAIL;
}

uint8_t* get_ble_session_key(void) {
    return g_ble_session.session_key;
}