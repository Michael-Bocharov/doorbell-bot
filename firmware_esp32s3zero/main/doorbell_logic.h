#ifndef DOORBELL_LOGIC_H
#define DOORBELL_LOGIC_H

/**
 * @brief Initialize GPIOs for doorbell logic.
 */
void doorbell_logic_init(void);

/**
 * @brief Trigger the door relay to open the door.
 */
void doorbell_logic_open_door(void);

#endif // DOORBELL_LOGIC_H
