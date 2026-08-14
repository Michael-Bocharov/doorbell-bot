#include "doorbell_logic.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "led_status.h"
#include "telegram_bot.h"

static const char *TAG = "doorbell_logic";

static int s_pin_ring_detector = 4;
static int s_pin_door_relay = 45;

/* Debounce: ignore repeat rings within this window (ms) */
#define RING_DEBOUNCE_MS 5000

static bool s_party_mode_active = false;
static uint32_t s_party_mode_duration_minutes = 120; // Default 2 hours
static TimerHandle_t s_party_mode_timer = NULL;
static TickType_t s_party_mode_start_tick = 0;

static void ring_detector_task(void *pvParameters) {
  esp_task_wdt_add(NULL);

  bool last_state = false;
  TickType_t last_ring_tick = 0;

  while (1) {
    esp_task_wdt_reset();

    bool current_state = gpio_get_level(s_pin_ring_detector);

    if (current_state && !last_state) {
      TickType_t now = xTaskGetTickCount();
      /* Debounce: skip if too soon after last ring */
      if ((now - last_ring_tick) > pdMS_TO_TICKS(RING_DEBOUNCE_MS)) {
        ESP_LOGI(TAG, "🔔 Doorbell is ringing!");
        led_status_set(LED_STATUS_RINGING);

        if (s_party_mode_active) {
          ESP_LOGI(TAG, "Party mode active - auto-unlocking door");
          telegram_bot_send_message("🔔 <b>Someone is at the door!</b>\n🎉 Party mode is active: automatically opening the door!");
          last_ring_tick = now;
          doorbell_logic_open_door();
          led_status_set(LED_STATUS_ONLINE);
        } else {
          telegram_bot_send_message("🔔 <b>Someone is at the door!</b>\n"
                                    "Reply <b>open</b> to unlock.");
          last_ring_tick = now;
          /* Flash ringing LED for 3 seconds, then back to online */
          vTaskDelay(pdMS_TO_TICKS(3000));
          led_status_set(LED_STATUS_ONLINE);
        }
      }
    }

    last_state = current_state;
    vTaskDelay(pdMS_TO_TICKS(100)); /* Poll every 100 ms */
  }
}


void doorbell_logic_init(int ring_pin, int relay_pin) {
  s_pin_ring_detector = ring_pin;
  s_pin_door_relay = relay_pin;

  /* Configure Ring Detector input */
  gpio_config_t ring_io_conf = {
      .pin_bit_mask = (1ULL << s_pin_ring_detector),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&ring_io_conf);

  /* Configure Door Relay output */
  gpio_config_t relay_io_conf = {
      .pin_bit_mask = (1ULL << s_pin_door_relay),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&relay_io_conf);
  gpio_set_level(s_pin_door_relay, 0);

  xTaskCreate(ring_detector_task, "ring_task", 4096, NULL, 5, NULL);
}

void doorbell_logic_open_door(void) {
  ESP_LOGI(TAG, "🔓 Opening door remotely!");
  gpio_set_level(s_pin_door_relay, 1);
  vTaskDelay(pdMS_TO_TICKS(2000));
  gpio_set_level(s_pin_door_relay, 0);
  ESP_LOGI(TAG, "Door relay closed.");
}

static void party_mode_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "Party mode expired.");
    s_party_mode_active = false;
    telegram_bot_send_message("ℹ️ Party mode has expired and is now disabled.");
}

void doorbell_logic_set_party_mode(bool active, uint32_t duration_minutes) {
    s_party_mode_active = active;
    if (active) {
        if (duration_minutes == 0) {
            duration_minutes = 120; // Default to 2 hours
        }
        s_party_mode_duration_minutes = duration_minutes;
        s_party_mode_start_tick = xTaskGetTickCount();

        if (s_party_mode_timer == NULL) {
            s_party_mode_timer = xTimerCreate("party_timer", pdMS_TO_TICKS(duration_minutes * 60 * 1000), pdFALSE, NULL, party_mode_timer_callback);
        } else {
            xTimerChangePeriod(s_party_mode_timer, pdMS_TO_TICKS(duration_minutes * 60 * 1000), 0);
        }
        if (s_party_mode_timer) {
            xTimerStart(s_party_mode_timer, 0);
        }
        ESP_LOGI(TAG, "Party mode enabled for %u minutes", (unsigned int)duration_minutes);
    } else {
        if (s_party_mode_timer) {
            xTimerStop(s_party_mode_timer, 0);
        }
        ESP_LOGI(TAG, "Party mode disabled");
    }
}

bool doorbell_logic_get_party_mode(void) {
    return s_party_mode_active;
}

uint32_t doorbell_logic_get_party_mode_remaining(void) {
    if (!s_party_mode_active) {
        return 0;
    }
    TickType_t elapsed = xTaskGetTickCount() - s_party_mode_start_tick;
    uint32_t duration_ticks = pdMS_TO_TICKS(s_party_mode_duration_minutes * 60 * 1000);
    if (elapsed >= duration_ticks) {
        return 0;
    }
    return (duration_ticks - elapsed) * portTICK_PERIOD_MS / 1000;
}

uint32_t doorbell_logic_get_party_mode_duration(void) {
    return s_party_mode_duration_minutes;
}

#ifdef UNIT_TESTING
void doorbell_logic_test_reset(void) {
    s_party_mode_active = false;
    s_party_mode_duration_minutes = 120;
    s_party_mode_timer = NULL;
    s_party_mode_start_tick = 0;
    s_pin_ring_detector = 4;
    s_pin_door_relay = 45;
}
#endif

