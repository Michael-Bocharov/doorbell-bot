#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Callback type for received Telegram commands.
 *
 * @param command The command text (e.g., "open")
 */
typedef void (*telegram_command_callback_t)(const char *command);

/**
 * @brief Initialize the Telegram Bot module.
 *
 * Stores the bot token and chat ID, then starts the polling task
 * that checks for new messages via getUpdates.
 *
 * @param bot_token  Telegram Bot API token
 * @param chat_id    Target chat/group ID for notifications
 * @param admin_id   Telegram User ID of the administrator
 * @param callback   Function called when a command is received
 */
void telegram_bot_init(const char *bot_token, const char *chat_id, const char *admin_id,
                       telegram_command_callback_t callback);

/**
 * @brief Send a text message to the configured Telegram chat.
 *
 * @param text  Message text to send
 * @return true on success, false on failure
 */
bool telegram_bot_send_message(const char *text);

/**
 * @brief Get the current whitelist.
 * @param whitelist  Pointer to an array to store whitelist IDs
 * @param max_users  Maximum number of users that can be stored in the array
 * @return Number of users placed in the array
 */
int telegram_bot_get_whitelist(int64_t *whitelist, int max_users);

/**
 * @brief Add a user to the whitelist.
 * @return true on success, false if list is full or user already exists
 */
bool telegram_bot_add_user(int64_t user_id);

/**
 * @brief Remove a user from the whitelist.
 * @return true on success, false if user not found
 */
bool telegram_bot_remove_user(int64_t user_id);

#endif // TELEGRAM_BOT_H
