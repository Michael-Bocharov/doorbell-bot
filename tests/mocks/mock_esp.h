#ifndef MOCK_ESP_H
#define MOCK_ESP_H

#include "esp_err.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "led_status.h"
#include "telegram_bot.h"
#include "nvs.h"

// Reset all mock states (GPIO, FreeRTOS, Telegram, LED, NVS)
void mock_esp_reset_all(void);

// --- FreeRTOS Tick Mock ---
void mock_esp_set_ticks(TickType_t ticks);
void mock_esp_tick_elapse(TickType_t elapse_ticks);

// --- GPIO Mock ---
void mock_esp_set_input_gpio_level(int gpio_num, int level);
int mock_esp_get_output_gpio_level(int gpio_num);
int mock_esp_get_gpio_set_level_count(int gpio_num);
void mock_esp_reset_gpio_stats(void);

// --- Telegram Bot Mock ---
void mock_esp_reset_telegram_calls(void);
int mock_esp_get_telegram_send_count(void);
const char *mock_esp_get_telegram_last_msg(void);

// --- LED Status Mock ---
void mock_esp_reset_led_calls(void);
led_status_t mock_esp_get_led_last_status(void);
int mock_esp_get_led_status_call_count(void);

// --- FreeRTOS Timers Mock ---
void mock_esp_trigger_timer_callback(TimerHandle_t xTimer);

// --- NVS Mock ---
void mock_esp_nvs_set_error(esp_err_t err);
void mock_esp_nvs_set_data(const void *data, size_t size);
const void *mock_esp_nvs_get_data(size_t *out_size);
int mock_esp_nvs_get_open_count(void);
int mock_esp_nvs_get_commit_count(void);

#endif // MOCK_ESP_H
