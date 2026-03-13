idf_component_register(SRCS "src/main.c" "src/led.c" "src/wifi_manager.c" "src/wifi_ap.c"
                      PRIV_REQUIRES esp_driver_gpio esp_wifi nvs_flash esp_netif esp_event
                       INCLUDE_DIRS "." "include")
