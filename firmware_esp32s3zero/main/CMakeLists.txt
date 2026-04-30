idf_component_register(SRCS "main.c" "led_status.c" "doorbell_logic.c" "telegram_bot.c"
                    INCLUDE_DIRS "."
                    REQUIRES nvs_flash esp_wifi esp_event esp_common esp_http_client esp_driver_gpio json
                    PRIV_REQUIRES mbedtls)
