#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdbool.h>

#define CONFIG_MAX_WIFI_SSID_LEN 32
#define CONFIG_MAX_WIFI_PASS_LEN 64
#define CONFIG_MAX_TG_TOKEN_LEN 128
#define CONFIG_MAX_TG_CHAT_ID_LEN 32
#define CONFIG_MAX_TG_ADMIN_ID_LEN 32

typedef struct {
    char wifi_ssid[CONFIG_MAX_WIFI_SSID_LEN];
    char wifi_password[CONFIG_MAX_WIFI_PASS_LEN];
    char tg_bot_token[CONFIG_MAX_TG_TOKEN_LEN];
    char tg_chat_id[CONFIG_MAX_TG_CHAT_ID_LEN];
    char tg_admin_id[CONFIG_MAX_TG_ADMIN_ID_LEN];
} device_config_t;

/**
 * @brief Initialize config manager and load config from NVS.
 * @return true if config was successfully loaded and is valid (e.g. has SSID), false otherwise.
 */
bool config_manager_load(device_config_t *config);

/**
 * @brief Save config to NVS.
 * @return true on success.
 */
bool config_manager_save(const device_config_t *config);

#endif // CONFIG_MANAGER_H
