#include "unity.h"
#include "doorbell_logic.h"
#include "mocks/mock_esp.h"
#include <string.h>

void setUp(void) {
    mock_esp_reset_all();
    doorbell_logic_test_reset();
}

void tearDown(void) {
    // Clean up
}

void test_party_mode_default_state(void) {
    TEST_ASSERT_FALSE(doorbell_logic_get_party_mode());
    TEST_ASSERT_EQUAL_UINT32(0, doorbell_logic_get_party_mode_remaining());
}

void test_party_mode_enable(void) {
    doorbell_logic_set_party_mode(true, 60);
    TEST_ASSERT_TRUE(doorbell_logic_get_party_mode());
    TEST_ASSERT_EQUAL_UINT32(60, doorbell_logic_get_party_mode_duration());
    // Remaining time right after activation (60 minutes = 3600 seconds)
    TEST_ASSERT_EQUAL_UINT32(3600, doorbell_logic_get_party_mode_remaining());
}

void test_party_mode_enable_zero_defaults_to_120_minutes(void) {
    doorbell_logic_set_party_mode(true, 0);
    TEST_ASSERT_TRUE(doorbell_logic_get_party_mode());
    TEST_ASSERT_EQUAL_UINT32(120, doorbell_logic_get_party_mode_duration());
    TEST_ASSERT_EQUAL_UINT32(7200, doorbell_logic_get_party_mode_remaining());
}

void test_party_mode_get_remaining_decreases_over_ticks(void) {
    doorbell_logic_set_party_mode(true, 10); // 10 minutes = 600 seconds
    
    // Simulate 3 minutes passing (3 * 60 * 1000 = 180000 ms = 180000 ticks if 1ms per tick)
    mock_esp_tick_elapse(180000);
    
    // Remaining time should be 7 minutes = 420 seconds
    TEST_ASSERT_EQUAL_UINT32(420, doorbell_logic_get_party_mode_remaining());
}

void test_party_mode_disable(void) {
    doorbell_logic_set_party_mode(true, 30);
    TEST_ASSERT_TRUE(doorbell_logic_get_party_mode());
    
    doorbell_logic_set_party_mode(false, 0);
    TEST_ASSERT_FALSE(doorbell_logic_get_party_mode());
    TEST_ASSERT_EQUAL_UINT32(0, doorbell_logic_get_party_mode_remaining());
}

void test_party_mode_expiration(void) {
    doorbell_logic_set_party_mode(true, 5); // 5 minutes = 300000 ms
    
    // Elapse less than 5 minutes (e.g. 4 minutes = 240000 ms)
    mock_esp_tick_elapse(240000);
    TEST_ASSERT_TRUE(doorbell_logic_get_party_mode());
    TEST_ASSERT_EQUAL_UINT32(0, mock_esp_get_telegram_send_count());

    // Elapse the remaining 1 minute (60000 ms) to cross the threshold (total 300000 ms)
    mock_esp_tick_elapse(60000);
    
    // Timer should have expired and callback executed
    TEST_ASSERT_FALSE(doorbell_logic_get_party_mode());
    TEST_ASSERT_EQUAL_UINT32(1, mock_esp_get_telegram_send_count());
    TEST_ASSERT_NOT_NULL(strstr(mock_esp_get_telegram_last_msg(), "expired"));
}

void test_open_door_triggers_gpio_and_leds(void) {
    mock_esp_reset_gpio_stats();
    
    doorbell_logic_open_door();
    
    // PIN_DOOR_RELAY is 45. Open door drives it HIGH, then delays 2s, then drives it LOW.
    // So gpio_set_level(45, 1) then gpio_set_level(45, 0) should be called.
    TEST_ASSERT_EQUAL_INT(2, mock_esp_get_gpio_set_level_count(45));
    TEST_ASSERT_EQUAL_INT(0, mock_esp_get_output_gpio_level(45)); // ended at 0
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_party_mode_default_state);
    RUN_TEST(test_party_mode_enable);
    RUN_TEST(test_party_mode_enable_zero_defaults_to_120_minutes);
    RUN_TEST(test_party_mode_get_remaining_decreases_over_ticks);
    RUN_TEST(test_party_mode_disable);
    RUN_TEST(test_party_mode_expiration);
    RUN_TEST(test_open_door_triggers_gpio_and_leds);
    return UNITY_END();
}
