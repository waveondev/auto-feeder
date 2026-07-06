#ifndef __GPIO_UTIL_H__
#define __GPIO_UTIL_H__

#include "driver/gpio.h"

//
//  LED State
//
typedef enum {
  LCS_BLE_EN_ADV_WIFI_NOT_WORKING = 0,
  LCS_BLE_EN_ADV_WIFI_EN_WORKING,
  LCS_IDLE_BLE_CONNECTED,
  LCS_WORKING_BLE_CONNECTED,
} LED_States;
#define BLINK_GPIO   14   // 네오픽셀 데이터 선이 연결된 GPIO 핀 번호
#ifndef EX_POWER
  #define EX_POWER 41
#endif
#define PIN_PUMP_ADC  6
#define PIN_PUMP_PWM  15
#ifndef PIN_HX711_DOUT
  #define PIN_HX711_DOUT 40
#endif
#ifndef PIN_HX711_SCK
  #define PIN_HX711_SCK 39
#endif

#define PIN_PKEY_STAT 47  // PKEY_STAT

#ifndef PIN_TOF0_I2C_SDA
#define PIN_TOF0_I2C_SDA 8
#endif
#ifndef PIN_TOF0_I2C_SCL
#define PIN_TOF0_I2C_SCL 9
#endif
#ifndef PIN_TOF0_INT
#define PIN_TOF0_INT 3
#endif

// 요청하신 핀 번호 정의
#define ISD_MOSI_PIN  11
#define ISD_MISO_PIN  13
#define ISD_SCLK_PIN  12
#define ISD_CS_PIN    10

void gpio_init(gpio_num_t num, gpio_mode_t mode, gpio_int_type_t int_type,gpio_isr_t func);
void gpio_toggle(gpio_num_t pin);
void gpio_setpin(gpio_num_t pin);
void gpio_resetpin(gpio_num_t pin);
int gpio_read(gpio_num_t pin) ;
#endif
