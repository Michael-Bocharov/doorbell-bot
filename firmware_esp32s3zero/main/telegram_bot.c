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
#include "nvs.h"
#include "nvs_flash.h"

static const char *TAG = "telegram_bot";

/* Configuration */
static char s_bot_token[128];
static char s_chat_id[32];
static telegram_command_callback_t s_cmd_callback = NULL;

/* Whitelist state */
#define MAX_WHITELIST_USERS 20
static int64_t s_whitelist[MAX_WHITELIST_USERS];
static int s_whitelist_count = 0;
static int64_t s_admin_id = 0;

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
/* Whitelist Management                                               */
/* ------------------------------------------------------------------ */
static void save_whitelist(void) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        nvs_set_blob(my_handle, "whitelist", s_whitelist, sizeof(int64_t) * s_whitelist_count);
        nvs_commit(my_handle);
        nvs_close(my_handle);
    }
}

static bool is_user_authorized(int64_t user_id) {
    if (user_id != 0 && user_id == s_admin_id) return true;
    for (int i = 0; i < s_whitelist_count; i++) {
        if (s_whitelist[i] == user_id) return true;
    }
    return false;
}

static bool add_user(int64_t user_id) {
    if (is_user_authorized(user_id)) return false; // Already there
    if (s_whitelist_count < MAX_WHITELIST_USERS) {
        s_whitelist[s_whitelist_count++] = user_id;
        save_whitelist();
        return true;
    }
    return false;
}

static bool remove_user(int64_t user_id) {
    for (int i = 0; i < s_whitelist_count; i++) {
        if (s_whitelist[i] == user_id) {
            for (int j = i; j < s_whitelist_count - 1; j++) {
                s_whitelist[j] = s_whitelist[j + 1];
            }
            s_whitelist_count--;
            save_whitelist();
            return true;
        }
    }
    return false;
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

        /* Extract sender ID */
        cJSON *from = cJSON_GetObjectItem(message, "from");
        int64_t from_id = 0;
        if (from) {
            cJSON *id_obj = cJSON_GetObjectItem(from, "id");
            if (cJSON_IsNumber(id_obj)) {
                from_id = (int64_t)id_obj->valuedouble;
            }
        }

        /* Check admin commands */
        if (from_id == s_admin_id && s_admin_id != 0) {
            if (strncmp(text, "/add ", 5) == 0) {
                int64_t new_user = atoll(text + 5);
                if (new_user != 0) {
                    if (add_user(new_user)) {
                        telegram_bot_send_message("✅ User added to whitelist.");
                    } else {
                        telegram_bot_send_message("⚠️ Could not add user (already exists or list full).");
                    }
                }
                continue;
            } else if (strncmp(text, "/remove ", 8) == 0) {
                int64_t del_user = atoll(text + 8);
                if (remove_user(del_user)) {
                    telegram_bot_send_message("✅ User removed from whitelist.");
                } else {
                    telegram_bot_send_message("⚠️ User not found in whitelist.");
                }
                continue;
            } else if (strcasecmp(text, "/list") == 0) {
                char list_msg[512] = "📋 <b>Whitelist:</b>\n";
                for (int i = 0; i < s_whitelist_count; i++) {
                    char user_str[32];
                    snprintf(user_str, sizeof(user_str), "- %lld\n", s_whitelist[i]);
                    strncat(list_msg, user_str, sizeof(list_msg) - strlen(list_msg) - 1);
                }
                if (s_whitelist_count == 0) strncat(list_msg, "<i>Empty</i>", sizeof(list_msg) - strlen(list_msg) - 1);
                telegram_bot_send_message(list_msg);
                continue;
            }
        }

        /* Check for "open" command (case-insensitive) */
        if (strcasecmp(text, "open") == 0 || strcasecmp(text, "/open") == 0) {
            if (is_user_authorized(from_id)) {
                if (s_cmd_callback) {
                    s_cmd_callback("OPEN");
                }
            } else {
                char msg[128];
                snprintf(msg, sizeof(msg), "⛔️ Unauthorized access attempt from ID: %lld", from_id);
                telegram_bot_send_message(msg);
                ESP_LOGW(TAG, "Unauthorized access attempt from %lld", from_id);
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

    s_admin_id = atoll(CONFIG_DOORBELL_TELEGRAM_ADMIN_ID);

    /* Load whitelist from NVS */
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err == ESP_OK) {
        size_t required_size = 0;
        err = nvs_get_blob(my_handle, "whitelist", NULL, &required_size);
        if (err == ESP_OK && required_size > 0 && required_size <= sizeof(s_whitelist)) {
            nvs_get_blob(my_handle, "whitelist", s_whitelist, &required_size);
            s_whitelist_count = required_size / sizeof(int64_t);
        }
        nvs_close(my_handle);
    }

    ESP_LOGI(TAG, "Telegram bot initialised (chat_id=%s, admin_id=%lld, whitelist_count=%d)", s_chat_id, s_admin_id, s_whitelist_count);

    /* Start the polling task with enough stack for TLS + JSON parsing */
    xTaskCreate(telegram_poll_task, "tg_poll", 8192, NULL, 5, NULL);
}
