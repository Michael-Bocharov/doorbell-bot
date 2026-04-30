#include "doorbell_logic.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_status.h"
#include "telegram_bot.h"

static const char *TAG = "doorbell_logic";

#define PIN_RING_DETECTOR 4
#define PIN_DOOR_RELAY 45

/* Debounce: ignore repeat rings within this window (ms) */
#define RING_DEBOUNCE_MS 5000

static void ring_detector_task(void *pvParameters) {
  bool last_state = false;
  TickType_t last_ring_tick = 0;

  while (1) {
    bool current_state = gpio_get_level(PIN_RING_DETECTOR);

    if (current_state && !last_state) {
      TickType_t now = xTaskGetTickCount();
      /* Debounce: skip if too soon after last ring */
      if ((now - last_ring_tick) > pdMS_TO_TICKS(RING_DEBOUNCE_MS)) {
        ESP_LOGI(TAG, "🔔 Doorbell is ringing!");
        led_status_set(LED_STATUS_RINGING);

        telegram_bot_send_message("🔔 <b>Someone is at the door!</b>\n"
                                  "Reply <b>open</b> to unlock.");

        last_ring_tick = now;

        /* Flash ringing LED for 3 seconds, then back to online */
        vTaskDelay(pdMS_TO_TICKS(3000));
        led_status_set(LED_STATUS_ONLINE);
      }
    }

    last_state = current_state;
    vTaskDelay(pdMS_TO_TICKS(100)); /* Poll every 100 ms */
  }
}

void doorbell_logic_init(void) {
  /* Configure Ring Detector input (GPIO 4) */
  gpio_config_t ring_io_conf = {
      .pin_bit_mask = (1ULL << PIN_RING_DETECTOR),
      .mode = GPIO_MODE_INPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_ENABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&ring_io_conf);

  /* Configure Door Relay output (GPIO 5) */
  gpio_config_t relay_io_conf = {
      .pin_bit_mask = (1ULL << PIN_DOOR_RELAY),
      .mode = GPIO_MODE_OUTPUT,
      .pull_up_en = GPIO_PULLUP_DISABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type = GPIO_INTR_DISABLE,
  };
  gpio_config(&relay_io_conf);
  gpio_set_level(PIN_DOOR_RELAY, 0);

  xTaskCreate(ring_detector_task, "ring_task", 4096, NULL, 5, NULL);
}

void doorbell_logic_open_door(void) {
  ESP_LOGI(TAG, "🔓 Opening door remotely!");
  gpio_set_level(PIN_DOOR_RELAY, 1);
  vTaskDelay(pdMS_TO_TICKS(2000));
  gpio_set_level(PIN_DOOR_RELAY, 0);
  ESP_LOGI(TAG, "Door relay closed.");
}
