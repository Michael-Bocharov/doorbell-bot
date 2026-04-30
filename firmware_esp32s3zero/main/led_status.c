#include "led_status.h"
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_err.h"

static const char *TAG = "led_status";

/* WS2812 RGB LED on GPIO 21 (Waveshare ESP32-S3-Zero) */
#define LED_STRIP_GPIO 21

static led_strip_handle_t led_strip;
static led_status_t current_status = LED_STATUS_CONNECTING;

static void led_task(void *pvParameters)
{
    bool led_on = false;
    while (1) {
        int blink_ms = 0;
        uint8_t r = 0, g = 0, b = 0;

        switch (current_status) {
            case LED_STATUS_CONNECTING:
                blink_ms = 500;        /* Medium blink */
                r = 50; g = 30;        /* Yellow */
                break;
            case LED_STATUS_ONLINE:
                blink_ms = 0;          /* Solid */
                g = 50;               /* Green */
                break;
            case LED_STATUS_RINGING:
                blink_ms = 100;        /* Fast blink */
                r = 50; g = 50; b = 50; /* White */
                break;
            case LED_STATUS_ERROR:
                blink_ms = 300;        /* Medium-fast blink */
                r = 50;               /* Red */
                break;
        }

        if (blink_ms > 0) {
            if (led_on) {
                led_strip_set_pixel(led_strip, 0, r, g, b);
            } else {
                led_strip_clear(led_strip);
            }
            led_strip_refresh(led_strip);
            led_on = !led_on;
            vTaskDelay(pdMS_TO_TICKS(blink_ms));
        } else {
            /* Solid colour */
            led_strip_set_pixel(led_strip, 0, r, g, b);
            led_strip_refresh(led_strip);
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

void led_status_init(void)
{
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_GPIO,
        .max_leds = 1,
    };

    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = 10 * 1000 * 1000, /* 10 MHz */
        .flags.with_dma = false,
    };
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    led_strip_clear(led_strip);

    xTaskCreate(led_task, "led_task", 2048, NULL, 5, NULL);
}

void led_status_set(led_status_t status)
{
    ESP_LOGI(TAG, "Status → %d", (int)status);
    current_status = status;
}
