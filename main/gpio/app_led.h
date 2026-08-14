#ifndef __APP_LED_H__
#define __APP_LED_H__
#include "esp_log.h"

#define PAIRING_BIT     (1<<15)
#define OTA_START_BIT   (1<<14)
#define HARDWARE_ERR_BIT (1<<13)
#define SENSE_ERR_BIT   (1<<12)
#define TOF_DETECT_BIT  (1<<11)
#define FOOD_LOW_BIT  (1<<10)
#define FOOD_EMPTY_BIT  (1<<9)
#define FOOD_DISCHARGE_BIT  (1<<8)
#define LOCK_MODE_BIT   (1<<7)
#define FEED_MODE_BIT   (1<<6)
#define SLID_MODE_BIT   (1<<5)
#define ACC_MODE_BIT   (1<<4)
#define FEED_ERROR_BIT   (1<<3)
#define SLID_ERROR_BIT   (1<<2)
#define ACC_ERROR_BIT   (1<<1)
void LED_Bright_Set(uint8_t value);
bool TOF_enable(void);
bool led_bit_status(uint16_t status);
void led_bit_disable(uint16_t disable);
void led_bit_enable(uint16_t enable);
void init_led_strip(void);
void set_led_clear(void) ;
void set_rgb_len_no_Breathing(uint8_t R, uint8_t G, uint8_t B, uint8_t W);
void Breathing_Setup(uint8_t enable, uint8_t step, 
                            uint8_t min_bright,  // 💡 최소 밝기 (0~255)
                            uint8_t max_bright,  // 💡 최대 밝기 (0~255)
                            uint8_t target_r,
                            uint8_t target_g,
                            uint8_t target_b,
                            uint8_t target_w);
void Breathing_Setup_Debug(uint8_t enable, uint8_t step, 
                            uint8_t min_bright,  // 💡 최소 밝기 (0~255)
                            uint8_t max_bright,  // 💡 최대 밝기 (0~255)
                            uint8_t target_r,
                            uint8_t target_g,
                            uint8_t target_b,
                            uint8_t target_w);                         
void LED_task_init(void);
bool TOF_enable(void);
bool ota_enable(void);
bool hardware_error_enable(void);
bool sense_enable(void);
bool pairing_enable(void);
bool food_low_enable(void);
void wifi_connect_success(void);
#endif
