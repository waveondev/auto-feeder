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
#define LED_RGB_GPIO   38   // 네오픽셀 데이터 선이 연결된 GPIO 핀 번호



#ifndef PIN_HX711_DOUT
  #define PIN_HX711_DOUT 40
#endif
#ifndef PIN_HX711_SCK
  #define PIN_HX711_SCK 39
#endif

#define PIN_PKEY_STAT 47


#ifndef IR_ENABLE
#define IR_ENABLE 9
#endif




#define SLIDING_PWM_IN 15
#define FEED_PWM_IN    17
#define ACUUM_PWM_IN   41

#define SLIDING_PWM_CW  16
#define FEED_PWM_CW     48
#define ACUUM_PWM_CW     18

#define SLIDING_A_SEN  42
#define SLIDING_B_SEN  12
#define FEED_A_SEN  10

#define AMT_ENABLE 21
#define IR_AMOUNT  4


#define F_ENABLE  14
#define IR_FOOD_OUT 5

#define VBUS_CHECK 13
#define VBUS_ADC  2


void gpio_init(gpio_num_t num, gpio_mode_t mode, gpio_int_type_t int_type,gpio_isr_t func);
void gpio_toggle(gpio_num_t pin);
void gpio_setpin(gpio_num_t pin);
void gpio_resetpin(gpio_num_t pin);
int gpio_read(gpio_num_t pin) ;
#endif
