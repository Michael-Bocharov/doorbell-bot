#include "config_manager.h"
#include <string.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "config_manager";
static const char *NVS_NAMESPACE = "config";
static const char *NVS_KEY = "device_cfg";

bool config_manager_load(device_config_t *config) {
    memset(config, 0, sizeof(device_config_t));
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Error (%s) opening NVS handle! Config not found.", esp_err_to_name(err));
        return false;
    }

    size_t required_size = sizeof(device_config_t);
    err = nvs_get_blob(my_handle, NVS_KEY, config, &required_size);
    nvs_close(my_handle);

    if (err == ESP_OK && required_size == sizeof(device_config_t)) {
        ESP_LOGI(TAG, "Config loaded from NVS");
        // Check if config is somewhat valid (has SSID)
        if (strlen(config->wifi_ssid) > 0) {
            return true;
        }
        ESP_LOGW(TAG, "Config loaded but WiFi SSID is empty.");
        return false;
    } else {
        ESP_LOGW(TAG, "Config not found in NVS or size mismatch (err=%s)", esp_err_to_name(err));
        return false;
    }
}

bool config_manager_save(const device_config_t *config) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error (%s) opening NVS handle!", esp_err_to_name(err));
        return false;
    }

    err = nvs_set_blob(my_handle, NVS_KEY, config, sizeof(device_config_t));
    if (err == ESP_OK) {
        err = nvs_commit(my_handle);
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Config saved to NVS successfully");
        } else {
            ESP_LOGE(TAG, "Error committing config to NVS: %s", esp_err_to_name(err));
        }
    } else {
        ESP_LOGE(TAG, "Error writing config blob to NVS: %s", esp_err_to_name(err));
    }
    
    nvs_close(my_handle);
    return err == ESP_OK;
}
