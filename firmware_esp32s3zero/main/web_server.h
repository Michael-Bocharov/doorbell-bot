#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <stdbool.h>

/**
 * @brief Start the web server.
 * @return true on success
 */
bool web_server_start(void);

/**
 * @brief Stop the web server.
 */
void web_server_stop(void);

#endif // WEB_SERVER_H
