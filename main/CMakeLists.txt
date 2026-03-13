idf_component_register(SRCS "hydro_mqtt.c" "ba234.c" "st7789.c" "hydroponic_board.c" "web_server.c" "settings_manager.c" "sht4x.c" "ultrasonic_sensor.c"
                    INCLUDE_DIRS "."
                    REQUIRES
                        esp_driver_gpio
                        esp_driver_spi
                        esp_driver_i2c
                        esp_driver_uart
                        esp_wifi
                        esp_http_server
                        nvs_flash
                        vfs
                        esp_event
                        esp_timer
                        esp_netif
                        kinetic_os
                    PRIV_REQUIRES esp-mqtt
                    EMBED_TXTFILES "webpage.html")
