#ifndef BLE_CRYPTO_H
#define BLE_CRYPTO_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 디바이스의 Ephemeral X25519 키쌍을 생성하고 공개키를 Base64로 반환.
 * 
 * @param out_base64_pubkey 공개키 문자열이 저장될 버퍼
 * @param max_len 버퍼의 최대 크기
 * @return esp_err_t 성공 시 ESP_OK
 */
esp_err_t ble_crypto_init_and_get_pubkey(char *out_base64_pubkey, size_t max_len);

/**
 * @brief 앱의 공개키를 수신하여 공유 비밀(S)을 계산하고 세션키를 도출.
 * 
 * @param app_base64_pubkey 앱으로부터 수신한 Base64 인코딩된 공개키
 * @return esp_err_t 성공 시 ESP_OK
 */
esp_err_t ble_crypto_compute_session_key(const char *app_base64_pubkey);

/**
 * @brief AES-256-GCM 알고리즘으로 수신된 데이터를 복호화.
 * 
 * @param ciphertext 복호화할 암호문
 * @param cipher_len 암호문의 길이
 * @param iv 12바이트 고유 Nonce
 * @param tag 16바이트 인증 태그
 * @param out_plaintext 복호화된 평문이 저장될 버퍼
 * @param out_len 복호화된 평문의 실제 길이
 * @return esp_err_t 성공 시 ESP_OK
 */
esp_err_t ble_crypto_decrypt(const uint8_t *ciphertext, size_t cipher_len,
                             const uint8_t *iv, const uint8_t *tag,
                             uint8_t *out_plaintext, size_t *out_len);

/**
 * @brief AES-256-GCM 알고리즘으로 송신할 데이터를 암호.
 * 
 * @param plaintext 암호화할 평문
 * @param plain_len 평문의 길이
 * @param out_iv 생성된 12바이트 Nonce가 저장될 버퍼
 * @param out_tag 생성된 16바이트 인증 태그가 저장될 버퍼
 * @param out_ciphertext 암호문이 저장될 버퍼
 * @return esp_err_t 성공 시 ESP_OK
 */
esp_err_t ble_crypto_encrypt(const uint8_t *plaintext, size_t plain_len,
                             uint8_t *out_iv, uint8_t *out_tag, uint8_t *out_ciphertext);

#ifdef __cplusplus
}
#endif

#endif // BLE_CRYPTO_H