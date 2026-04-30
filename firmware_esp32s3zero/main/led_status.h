#ifndef LED_STATUS_H
#define LED_STATUS_H

#include "led_strip.h"

typedef enum {
    LED_STATUS_CONNECTING,  // Yellow blink  — WiFi connecting
    LED_STATUS_ONLINE,      // Green solid   — connected & polling Telegram
    LED_STATUS_RINGING,     // White fast blink — doorbell ringing
    LED_STATUS_ERROR        // Red blink     — error state
} led_status_t;

/**
 * @brief Initialize the WS2812 LED strip and start the status task.
 */
void led_status_init(void);

/**
 * @brief Update the current system status shown on the LED.
 *
 * @param status New status
 */
void led_status_set(led_status_t status);

#endif // LED_STATUS_H
