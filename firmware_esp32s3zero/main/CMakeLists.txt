idf_component_register(SRCS "main.c" "led_status.c" "doorbell_logic.c" "telegram_bot.c" "config_manager.c" "web_server.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_event esp_common esp_http_client esp_driver_gpio json esp_http_server esp_netif
                    PRIV_REQUIRES mbedtls)
