#include "telegram_bot.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"

static const char *TAG = "telegram_bot";

/* Configuration */
static char s_bot_token[128];
static char s_chat_id[32];
static telegram_command_callback_t s_cmd_callback = NULL;

/* Polling state */
static int64_t s_last_update_id = 0;

/* HTTP response buffer */
#define HTTP_RECV_BUF_SIZE 2048
static char s_recv_buf[HTTP_RECV_BUF_SIZE];
static int  s_recv_len = 0;

/* ------------------------------------------------------------------ */
/* HTTP event handler — accumulates response body                     */
/* ------------------------------------------------------------------ */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (s_recv_len + evt->data_len < HTTP_RECV_BUF_SIZE - 1) {
                memcpy(s_recv_buf + s_recv_len, evt->data, evt->data_len);
                s_recv_len += evt->data_len;
                s_recv_buf[s_recv_len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

/* ------------------------------------------------------------------ */
/* Send a message to the configured Telegram chat                     */
/* ------------------------------------------------------------------ */
bool telegram_bot_send_message(const char *text)
{
    char url[256];
    snprintf(url, sizeof(url),
             "https://api.telegram.org/bot%s/sendMessage", s_bot_token);

    /* Build JSON body */
    cJSON *body = cJSON_CreateObject();
    cJSON_AddStringToObject(body, "chat_id", s_chat_id);
    cJSON_AddStringToObject(body, "text", text);
    cJSON_AddStringToObject(body, "parse_mode", "HTML");
    char *json_str = cJSON_PrintUnformatted(body);
    cJSON_Delete(body);

    s_recv_len = 0;
    s_recv_buf[0] = '\0';

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 10000,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, json_str, strlen(json_str));

    esp_err_t err = esp_http_client_perform(client);
    bool ok = false;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "sendMessage status=%d", status);
        if (status == 200) {
            ok = true;
        } else {
            ESP_LOGE(TAG, "sendMessage error response: %s", s_recv_buf);
        }
    } else {
        ESP_LOGE(TAG, "sendMessage failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(json_str);
    return ok;
}

/* ------------------------------------------------------------------ */
/* Parse getUpdates response and invoke callback for "open" commands   */
/* ------------------------------------------------------------------ */
static void parse_updates(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) return;

    cJSON *ok_field = cJSON_GetObjectItem(root, "ok");
    if (!cJSON_IsTrue(ok_field)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *result = cJSON_GetObjectItem(root, "result");
    if (!cJSON_IsArray(result)) {
        cJSON_Delete(root);
        return;
    }

    cJSON *update = NULL;
    cJSON_ArrayForEach(update, result) {
        /* Track update_id to avoid processing the same message twice */
        cJSON *uid = cJSON_GetObjectItem(update, "update_id");
        if (cJSON_IsNumber(uid)) {
            int64_t id = (int64_t)uid->valuedouble;
            if (id > s_last_update_id) {
                s_last_update_id = id;
            }
        }

        /* Extract message text */
        cJSON *message = cJSON_GetObjectItem(update, "message");
        if (!message) continue;

        cJSON *text_obj = cJSON_GetObjectItem(message, "text");
        if (!cJSON_IsString(text_obj)) continue;

        const char *text = text_obj->valuestring;
        ESP_LOGI(TAG, "Received message: %s", text);

        /* Check for "open" command (case-insensitive) */
        if (strcasecmp(text, "open") == 0 || strcasecmp(text, "/open") == 0) {
            if (s_cmd_callback) {
                s_cmd_callback("OPEN");
            }
        }
    }

    cJSON_Delete(root);
}

/* ------------------------------------------------------------------ */
/* Polling task — calls getUpdates every 2 seconds                    */
/* ------------------------------------------------------------------ */
static void telegram_poll_task(void *pvParameters)
{
    char url[320];

    /* Small initial delay to allow WiFi to stabilise */
    vTaskDelay(pdMS_TO_TICKS(2000));

    ESP_LOGI(TAG, "Telegram polling task started");

    while (1) {
        /* Build URL with offset to only get new updates */
        snprintf(url, sizeof(url),
                 "https://api.telegram.org/bot%s/getUpdates?offset=%lld&timeout=5&allowed_updates=[\"message\"]",
                 s_bot_token, s_last_update_id + 1);

        s_recv_len = 0;
        s_recv_buf[0] = '\0';

        esp_http_client_config_t config = {
            .url = url,
            .method = HTTP_METHOD_GET,
            .event_handler = http_event_handler,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = 15000, /* long-poll: 5s server + 10s margin */
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_err_t err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status = esp_http_client_get_status_code(client);
            if (status == 200 && s_recv_len > 0) {
                parse_updates(s_recv_buf);
            }
        } else {
            ESP_LOGW(TAG, "getUpdates failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);

        /* Short pause before next poll cycle */
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

/* ------------------------------------------------------------------ */
/* Public: Initialise the Telegram Bot module                         */
/* ------------------------------------------------------------------ */
void telegram_bot_init(const char *bot_token, const char *chat_id,
                       telegram_command_callback_t callback)
{
    strncpy(s_bot_token, bot_token, sizeof(s_bot_token) - 1);
    strncpy(s_chat_id, chat_id, sizeof(s_chat_id) - 1);
    s_cmd_callback = callback;

    ESP_LOGI(TAG, "Telegram bot initialised (chat_id=%s)", s_chat_id);

    /* Start the polling task with enough stack for TLS + JSON parsing */
    xTaskCreate(telegram_poll_task, "tg_poll", 8192, NULL, 5, NULL);
}
