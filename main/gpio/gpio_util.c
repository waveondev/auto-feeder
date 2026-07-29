#include "gpio_util.h"


void gpio_resetpin(gpio_num_t pin) {
    gpio_set_level(pin, 0);            // 반대값 쓰기
}

void gpio_setpin(gpio_num_t pin) {
    gpio_set_level(pin, 1);            // 반대값 쓰기
}

void gpio_toggle(gpio_num_t pin) {
    int level = gpio_get_level(pin);        // 현재 상태 읽기
    gpio_set_level(pin, !level);            // 반대값 쓰기
}
int gpio_read(gpio_num_t pin) {
    int level = gpio_get_level(pin);        // 현재 상태 읽기
    return level;            // 반대값 쓰기
}
void gpio_init(gpio_num_t num, gpio_mode_t mode, gpio_int_type_t int_type,gpio_isr_t func)
{
    gpio_config_t config = {0};
    config.pin_bit_mask = BIT64(num);

    if(int_type != GPIO_INTR_DISABLE)
    {
        config.mode = GPIO_MODE_INPUT;
        config.intr_type = int_type;
        gpio_config(&config);

        gpio_install_isr_service(ESP_INTR_FLAG_LEVEL1);
        gpio_set_intr_type(num, GPIO_INTR_ANYEDGE);
        gpio_isr_handler_add(num, func, (void*)num);
    }
    else
    {
        config.mode = mode;
        config.pull_down_en = 0;
        config.pull_up_en = 0;
        gpio_config(&config);
    }
}



