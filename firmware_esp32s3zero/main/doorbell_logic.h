#ifndef DOORBELL_LOGIC_H
#define DOORBELL_LOGIC_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Initialize GPIOs for doorbell logic.
 */
void doorbell_logic_init(void);

/**
 * @brief Trigger the door relay to open the door.
 */
void doorbell_logic_open_door(void);

/**
 * @brief Enable or disable party mode.
 * @param active true to enable, false to disable
 * @param duration_minutes duration in minutes (if active is true)
 */
void doorbell_logic_set_party_mode(bool active, uint32_t duration_minutes);

/**
 * @brief Check if party mode is active.
 * @return true if active
 */
bool doorbell_logic_get_party_mode(void);

/**
 * @brief Get remaining time of party mode in seconds.
 * @return remaining seconds, or 0 if inactive
 */
uint32_t doorbell_logic_get_party_mode_remaining(void);

/**
 * @brief Get the configured duration in minutes.
 * @return duration in minutes
 */
uint32_t doorbell_logic_get_party_mode_duration(void);

#endif // DOORBELL_LOGIC_H
