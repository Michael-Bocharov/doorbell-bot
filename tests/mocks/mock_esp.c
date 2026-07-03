#include "mock_esp.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "esp_err.h"

// --- Global variables for mock tracking ---
static TickType_t s_current_ticks = 0;

#define MAX_GPIOS 64
static int s_input_gpio_levels[MAX_GPIOS] = {0};
static int s_output_gpio_levels[MAX_GPIOS] = {0};
static int s_gpio_set_level_counts[MAX_GPIOS] = {0};

static int s_telegram_send_count = 0;
static char s_telegram_last_msg[512] = {0};

static led_status_t s_led_last_status = LED_STATUS_CONNECTING;
static int s_led_status_call_count = 0;

// --- FreeRTOS Timer Mocks ---
#define MAX_TIMERS 5
typedef struct {
    char name[32];
    TickType_t period;
    bool active;
    bool auto_reload;
    void *timer_id;
    TimerCallbackFunction_t callback;
    TickType_t start_ticks;
} mock_timer_t;

static mock_timer_t s_timers[MAX_TIMERS];
static int s_timer_count = 0;

// --- NVS Mock ---
static esp_err_t s_nvs_err = ESP_OK;
static uint8_t s_nvs_data[512] = {0};
static size_t s_nvs_data_size = 0;
static int s_nvs_open_count = 0;
static int s_nvs_commit_count = 0;

// --- Reset implementation ---
void mock_esp_reset_all(void) {
    s_current_ticks = 0;
    memset(s_input_gpio_levels, 0, sizeof(s_input_gpio_levels));
    memset(s_output_gpio_levels, 0, sizeof(s_output_gpio_levels));
    memset(s_gpio_set_level_counts, 0, sizeof(s_gpio_set_level_counts));
    s_telegram_send_count = 0;
    s_telegram_last_msg[0] = '\0';
    s_led_last_status = LED_STATUS_CONNECTING;
    s_led_status_call_count = 0;
    
    memset(s_timers, 0, sizeof(s_timers));
    s_timer_count = 0;

    s_nvs_err = ESP_OK;
    memset(s_nvs_data, 0, sizeof(s_nvs_data));
    s_nvs_data_size = 0;
    s_nvs_open_count = 0;
    s_nvs_commit_count = 0;
}

// --- esp_err_to_name implementation ---
const char *esp_err_to_name(esp_err_t code) {
    switch (code) {
        case ESP_OK: return "ESP_OK";
        case ESP_FAIL: return "ESP_FAIL";
        case ESP_ERR_NOT_FOUND: return "ESP_ERR_NOT_FOUND";
        default: return "ESP_ERR_UNKNOWN";
    }
}

// --- FreeRTOS Tick Mock ---
void mock_esp_set_ticks(TickType_t ticks) {
    s_current_ticks = ticks;
}

void mock_esp_tick_elapse(TickType_t elapse_ticks) {
    s_current_ticks += elapse_ticks;
    
    // Check and trigger active timers that expired
    for (int i = 0; i < s_timer_count; i++) {
        if (s_timers[i].active) {
            TickType_t elapsed = s_current_ticks - s_timers[i].start_ticks;
            if (elapsed >= s_timers[i].period) {
                // Timer expired!
                s_timers[i].active = s_timers[i].auto_reload;
                if (s_timers[i].auto_reload) {
                    s_timers[i].start_ticks = s_current_ticks;
                }
                if (s_timers[i].callback) {
                    s_timers[i].callback(&s_timers[i]);
                }
            }
        }
    }
}

TickType_t xTaskGetTickCount(void) {
    return s_current_ticks;
}

void vTaskDelay(const TickType_t xTicksToDelay) {
    mock_esp_tick_elapse(xTicksToDelay);
}

BaseType_t xTaskCreate(TaskFunction_t pxTaskCode,
                       const char * const pcName,
                       const uint32_t usStackDepth,
                       void * const pvParameters,
                       UBaseType_t uxPriority,
                       TaskHandle_t * const pxCreatedTask) {
    // Return success but don't spawn host threads to run main firmware loops
    // in synchronous unit tests unless requested.
    if (pxCreatedTask) {
        *pxCreatedTask = (TaskHandle_t)1;
    }
    return pdTRUE;
}

// --- GPIO Mock ---
void mock_esp_set_input_gpio_level(int gpio_num, int level) {
    if (gpio_num >= 0 && gpio_num < MAX_GPIOS) {
        s_input_gpio_levels[gpio_num] = level;
    }
}

int mock_esp_get_output_gpio_level(int gpio_num) {
    if (gpio_num >= 0 && gpio_num < MAX_GPIOS) {
        return s_output_gpio_levels[gpio_num];
    }
    return 0;
}

int mock_esp_get_gpio_set_level_count(int gpio_num) {
    if (gpio_num >= 0 && gpio_num < MAX_GPIOS) {
        return s_gpio_set_level_counts[gpio_num];
    }
    return 0;
}

void mock_esp_reset_gpio_stats(void) {
    memset(s_gpio_set_level_counts, 0, sizeof(s_gpio_set_level_counts));
}

esp_err_t gpio_config(const gpio_config_t *config) {
    return ESP_OK;
}

esp_err_t gpio_set_level(int gpio_num, uint32_t level) {
    if (gpio_num >= 0 && gpio_num < MAX_GPIOS) {
        s_output_gpio_levels[gpio_num] = level;
        s_gpio_set_level_counts[gpio_num]++;
    }
    return ESP_OK;
}

int gpio_get_level(int gpio_num) {
    if (gpio_num >= 0 && gpio_num < MAX_GPIOS) {
        return s_input_gpio_levels[gpio_num];
    }
    return 0;
}

// --- Telegram Bot Mock ---
void mock_esp_reset_telegram_calls(void) {
    s_telegram_send_count = 0;
    s_telegram_last_msg[0] = '\0';
}

int mock_esp_get_telegram_send_count(void) {
    return s_telegram_send_count;
}

const char *mock_esp_get_telegram_last_msg(void) {
    return s_telegram_last_msg;
}

bool telegram_bot_send_message(const char *text) {
    s_telegram_send_count++;
    if (text) {
        strncpy(s_telegram_last_msg, text, sizeof(s_telegram_last_msg) - 1);
        s_telegram_last_msg[sizeof(s_telegram_last_msg) - 1] = '\0';
    }
    return true;
}

// --- LED Status Mock ---
void mock_esp_reset_led_calls(void) {
    s_led_last_status = LED_STATUS_CONNECTING;
    s_led_status_call_count = 0;
}

led_status_t mock_esp_get_led_last_status(void) {
    return s_led_last_status;
}

int mock_esp_get_led_status_call_count(void) {
    return s_led_status_call_count;
}

void led_status_set(led_status_t status) {
    s_led_last_status = status;
    s_led_status_call_count++;
}

// --- FreeRTOS Timers Mock ---
TimerHandle_t xTimerCreate(const char * const pcTimerName,
                           const TickType_t xTimerPeriodInTicks,
                           const UBaseType_t uxAutoReload,
                           void * const pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction) {
    if (s_timer_count >= MAX_TIMERS) {
        return NULL;
    }
    mock_timer_t *timer = &s_timers[s_timer_count++];
    strncpy(timer->name, pcTimerName ? pcTimerName : "", sizeof(timer->name) - 1);
    timer->period = xTimerPeriodInTicks;
    timer->auto_reload = (uxAutoReload == pdTRUE);
    timer->timer_id = pvTimerID;
    timer->callback = pxCallbackFunction;
    timer->active = false;
    return (TimerHandle_t)timer;
}

BaseType_t xTimerChangePeriod(TimerHandle_t xTimer,
                              const TickType_t xNewPeriod,
                              const TickType_t xTicksToWait) {
    if (!xTimer) return pdFALSE;
    mock_timer_t *timer = (mock_timer_t *)xTimer;
    timer->period = xNewPeriod;
    timer->start_ticks = s_current_ticks;
    timer->active = true;
    return pdTRUE;
}

BaseType_t xTimerStart(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    if (!xTimer) return pdFALSE;
    mock_timer_t *timer = (mock_timer_t *)xTimer;
    timer->start_ticks = s_current_ticks;
    timer->active = true;
    return pdTRUE;
}

BaseType_t xTimerStop(TimerHandle_t xTimer, const TickType_t xTicksToWait) {
    if (!xTimer) return pdFALSE;
    mock_timer_t *timer = (mock_timer_t *)xTimer;
    timer->active = false;
    return pdTRUE;
}

void mock_esp_trigger_timer_callback(TimerHandle_t xTimer) {
    if (!xTimer) return;
    mock_timer_t *timer = (mock_timer_t *)xTimer;
    if (timer->callback) {
        timer->callback(xTimer);
    }
}

// --- NVS Mock Implementation ---
void mock_esp_nvs_set_error(esp_err_t err) {
    s_nvs_err = err;
}

void mock_esp_nvs_set_data(const void *data, size_t size) {
    if (size > sizeof(s_nvs_data)) size = sizeof(s_nvs_data);
    s_nvs_data_size = size;
    if (data && size > 0) {
        memcpy(s_nvs_data, data, size);
    } else {
        memset(s_nvs_data, 0, sizeof(s_nvs_data));
    }
}

const void *mock_esp_nvs_get_data(size_t *out_size) {
    if (out_size) *out_size = s_nvs_data_size;
    return s_nvs_data;
}

int mock_esp_nvs_get_open_count(void) {
    return s_nvs_open_count;
}

int mock_esp_nvs_get_commit_count(void) {
    return s_nvs_commit_count;
}

esp_err_t nvs_open(const char *name, nvs_open_mode_t open_mode, nvs_handle_t *out_handle) {
    s_nvs_open_count++;
    if (s_nvs_err != ESP_OK) {
        return s_nvs_err;
    }
    if (out_handle) {
        *out_handle = 42; // some mock handle ID
    }
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out_value, size_t *length) {
    if (s_nvs_err != ESP_OK) {
        return s_nvs_err;
    }
    if (s_nvs_data_size == 0) {
        return ESP_ERR_NOT_FOUND;
    }
    if (out_value == NULL) {
        if (length) *length = s_nvs_data_size;
        return ESP_OK;
    }
    if (length) {
        if (*length < s_nvs_data_size) {
            return ESP_ERR_INVALID_SIZE;
        }
        *length = s_nvs_data_size;
    }
    memcpy(out_value, s_nvs_data, s_nvs_data_size);
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value, size_t length) {
    if (s_nvs_err != ESP_OK) {
        return s_nvs_err;
    }
    if (length > sizeof(s_nvs_data)) {
        return ESP_ERR_NO_MEM;
    }
    s_nvs_data_size = length;
    memcpy(s_nvs_data, value, length);
    return ESP_OK;
}

esp_err_t nvs_commit(nvs_handle_t handle) {
    s_nvs_commit_count++;
    return ESP_OK;
}

void nvs_close(nvs_handle_t handle) {
    // Closed mock handle
}
