#ifndef MOCK_GPIO_H
#define MOCK_GPIO_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define GPIO_MODE_INPUT      1
#define GPIO_MODE_OUTPUT     2

#define GPIO_PULLUP_DISABLE   0
#define GPIO_PULLUP_ENABLE    1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_PULLDOWN_ENABLE  1

#define GPIO_INTR_DISABLE     0

typedef struct {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;

esp_err_t gpio_config(const gpio_config_t *config);
esp_err_t gpio_set_level(int gpio_num, uint32_t level);
int gpio_get_level(int gpio_num);

#endif // MOCK_GPIO_H
