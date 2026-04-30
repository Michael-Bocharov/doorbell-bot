#ifndef TELEGRAM_BOT_H
#define TELEGRAM_BOT_H

#include <stdbool.h>

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
 * @param callback   Function called when a command is received
 */
void telegram_bot_init(const char *bot_token, const char *chat_id,
                       telegram_command_callback_t callback);

/**
 * @brief Send a text message to the configured Telegram chat.
 *
 * @param text  Message text to send
 * @return true on success, false on failure
 */
bool telegram_bot_send_message(const char *text);

#endif // TELEGRAM_BOT_H
