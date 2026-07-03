#include "unity.h"
#include "config_manager.h"
#include "mocks/mock_esp.h"
#include <string.h>

void setUp(void) {
    mock_esp_reset_all();
}

void tearDown(void) {
    // Clean up
}

void test_config_load_empty_nvs_fails(void) {
    device_config_t config;
    mock_esp_nvs_set_error(ESP_ERR_NOT_FOUND);
    
    bool result = config_manager_load(&config);
    
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(1, mock_esp_nvs_get_open_count());
}

void test_config_load_empty_ssid_fails(void) {
    device_config_t mock_data;
    memset(&mock_data, 0, sizeof(device_config_t));
    strcpy(mock_data.wifi_password, "somepassword");
    strcpy(mock_data.tg_bot_token, "sometoken");
    // wifi_ssid is left empty
    
    mock_esp_nvs_set_data(&mock_data, sizeof(device_config_t));
    
    device_config_t loaded_config;
    bool result = config_manager_load(&loaded_config);
    
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_STRING("", loaded_config.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("somepassword", loaded_config.wifi_password);
}

void test_config_load_valid_config_success(void) {
    device_config_t mock_data;
    memset(&mock_data, 0, sizeof(device_config_t));
    strcpy(mock_data.wifi_ssid, "MyHomeWiFi");
    strcpy(mock_data.wifi_password, "supersecure");
    strcpy(mock_data.tg_bot_token, "12345678:AAF-bot");
    strcpy(mock_data.tg_chat_id, "98765432");
    strcpy(mock_data.tg_admin_id, "12345");
    
    mock_esp_nvs_set_data(&mock_data, sizeof(device_config_t));
    
    device_config_t loaded_config;
    bool result = config_manager_load(&loaded_config);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_STRING("MyHomeWiFi", loaded_config.wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("supersecure", loaded_config.wifi_password);
    TEST_ASSERT_EQUAL_STRING("12345678:AAF-bot", loaded_config.tg_bot_token);
    TEST_ASSERT_EQUAL_STRING("98765432", loaded_config.tg_chat_id);
    TEST_ASSERT_EQUAL_STRING("12345", loaded_config.tg_admin_id);
    // Unassigned GPIOs in mock database default to 4 and 45 respectively
    TEST_ASSERT_EQUAL_INT(4, loaded_config.gpio_ring_detector);
    TEST_ASSERT_EQUAL_INT(45, loaded_config.gpio_door_relay);
}

void test_config_save_success(void) {
    device_config_t config_to_save;
    memset(&config_to_save, 0, sizeof(device_config_t));
    strcpy(config_to_save.wifi_ssid, "SaveTestSSID");
    strcpy(config_to_save.wifi_password, "SaveTestPass");
    config_to_save.gpio_ring_detector = 12;
    config_to_save.gpio_door_relay = 23;
    
    bool result = config_manager_save(&config_to_save);
    
    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_EQUAL_INT(1, mock_esp_nvs_get_open_count());
    TEST_ASSERT_EQUAL_INT(1, mock_esp_nvs_get_commit_count());
    
    // Read directly from mock buffer to verify it was serialized correctly
    size_t size = 0;
    const device_config_t *saved_data = mock_esp_nvs_get_data(&size);
    TEST_ASSERT_EQUAL_INT(sizeof(device_config_t), size);
    TEST_ASSERT_EQUAL_STRING("SaveTestSSID", saved_data->wifi_ssid);
    TEST_ASSERT_EQUAL_STRING("SaveTestPass", saved_data->wifi_password);
    TEST_ASSERT_EQUAL_INT(12, saved_data->gpio_ring_detector);
    TEST_ASSERT_EQUAL_INT(23, saved_data->gpio_door_relay);
}

void test_config_save_open_fails(void) {
    device_config_t config_to_save;
    memset(&config_to_save, 0, sizeof(device_config_t));
    strcpy(config_to_save.wifi_ssid, "SaveTestSSID");
    
    mock_esp_nvs_set_error(ESP_FAIL);
    
    bool result = config_manager_save(&config_to_save);
    
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(0, mock_esp_nvs_get_commit_count());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_config_load_empty_nvs_fails);
    RUN_TEST(test_config_load_empty_ssid_fails);
    RUN_TEST(test_config_load_valid_config_success);
    RUN_TEST(test_config_save_success);
    RUN_TEST(test_config_save_open_fails);
    return UNITY_END();
}
