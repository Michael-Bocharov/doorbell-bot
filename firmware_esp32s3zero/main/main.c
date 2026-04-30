#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "driver/gpio.h"

#include "led_status.h"
#include "doorbell_logic.h"
#include "telegram_bot.h"

static const char *TAG = "main";

/* Event group bit that signals a successful WiFi connection */
static const int WIFI_CONNECTED_BIT = BIT0;
static EventGroupHandle_t s_wifi_event_group;

/* ------------------------------------------------------------------ */
/* Telegram command callback — called from the polling task            */
/* ------------------------------------------------------------------ */
static void on_telegram_command(const char *command)
{
    if (strcmp(command, "OPEN") == 0) {
        ESP_LOGI(TAG, "Open command received from Telegram");
        doorbell_logic_open_door();
        telegram_bot_send_message("✅ Door opened!");
    }
}

/* ------------------------------------------------------------------ */
/* WiFi / IP event handler  (runs on sys_evt task — keep it light!)    */
/* ------------------------------------------------------------------ */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "WiFi disconnected — reconnecting…");
        led_status_set(LED_STATUS_CONNECTING);
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected — IP: " IPSTR, IP2STR(&event->ip_info.ip));
        led_status_set(LED_STATUS_ONLINE);
        /* Signal the network-ready task — do NOT call HTTPS from here */
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ------------------------------------------------------------------ */
/* Task that waits for WiFi, then starts Telegram bot                  */
/* (Has its own large stack so TLS handshake won't overflow)           */
/* ------------------------------------------------------------------ */
static void network_ready_task(void *pvParameters)
{
    /* Block until WiFi is connected */
    xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    ESP_LOGI(TAG, "WiFi connected — starting Telegram bot");
    telegram_bot_init(CONFIG_DOORBELL_TELEGRAM_BOT_TOKEN,
                      CONFIG_DOORBELL_TELEGRAM_CHAT_ID,
                      on_telegram_command);

    /* Send the startup notification (this is the heavy HTTPS call) */
    telegram_bot_send_message("🟢 DoorBell device is online!");

    /* Task done — delete itself */
    vTaskDelete(NULL);
}

/* ------------------------------------------------------------------ */
/* WiFi station initialisation                                         */
/* ------------------------------------------------------------------ */
static void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = CONFIG_DOORBELL_WIFI_SSID,
            .password = CONFIG_DOORBELL_WIFI_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi STA started — connecting to %s …",
             CONFIG_DOORBELL_WIFI_SSID);
}

/* ------------------------------------------------------------------ */
/* BOOT button monitor — long-press erases NVS and restarts            */
/* ------------------------------------------------------------------ */
static void button_monitor_task(void *pvParameters)
{
    gpio_reset_pin(GPIO_NUM_0);
    gpio_set_direction(GPIO_NUM_0, GPIO_MODE_INPUT);
    gpio_set_pull_mode(GPIO_NUM_0, GPIO_PULLUP_ONLY);

    uint32_t press_start = 0;

    while (1) {
        if (gpio_get_level(GPIO_NUM_0) == 0) {
            if (press_start == 0) {
                press_start = xTaskGetTickCount();
            } else if ((xTaskGetTickCount() - press_start) > pdMS_TO_TICKS(5000)) {
                ESP_LOGW(TAG, "BOOT long-press — factory reset!");
                led_status_set(LED_STATUS_ERROR);
                nvs_flash_erase();
                esp_restart();
            }
        } else {
            press_start = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

/* ------------------------------------------------------------------ */
/* app_main                                                            */
/* ------------------------------------------------------------------ */
void app_main(void)
{
    /* Initialise NVS */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    }

    /* LED status indicator */
    led_status_init();
    led_status_set(LED_STATUS_CONNECTING);

    /* Doorbell GPIO logic */
    doorbell_logic_init();

    /* Connect to WiFi */
    wifi_init_sta();

    /* Start a dedicated task to init Telegram after WiFi connects
       (needs 10 KB stack for TLS handshake + JSON) */
    xTaskCreate(network_ready_task, "net_ready", 10240, NULL, 5, NULL);

    /* BOOT button monitor (factory-reset on long-press) */
    xTaskCreate(button_monitor_task, "btn_mon", 2048, NULL, 5, NULL);
}
