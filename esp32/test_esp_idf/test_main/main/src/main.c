// Pull in the core FreeRTOS definitions used by ESP-IDF.
#include "freertos/FreeRTOS.h"
// Gives us vTaskDelay(), which pauses the current task.
#include "freertos/task.h"
// Defines esp_err_t and ESP_ERROR_CHECK().
#include "esp_err.h"
// Lets us create and use the ESP-IDF event loop.
#include "esp_event.h"
// Provides the network interface layer used by Wi-Fi.
#include "esp_netif.h"
// Provides non-volatile storage initialization for Wi-Fi and system data.
#include "nvs_flash.h"
// Exposes values generated from menuconfig, such as CONFIG_BLINK_PERIOD.
#include "sdkconfig.h"

// Public API for our LED module.
#include "led.h"
// Public API for our Wi-Fi module.
#include "wifi_manager.h"
// Public API for our Wi-Fi SoftAP module.
#include "wifi_ap.h"

// Give the menuconfig blink period a simpler local name.
#define BLINK_PERIOD_MS CONFIG_BLINK_PERIOD

// app_main() is the real entry point for an ESP-IDF application.
void app_main(void)
{
    // Try to initialize NVS because Wi-Fi depends on it.
    esp_err_t ret = nvs_flash_init();
    // If NVS is full or from an old format, erase it and initialize again.
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // Clear the flash region used by NVS.
        ESP_ERROR_CHECK(nvs_flash_erase());
        // Retry initialization after erase.
        ret = nvs_flash_init();
    }
    // Stop immediately if NVS still failed to initialize.
    ESP_ERROR_CHECK(ret);

    // Initialize the TCP/IP stack used by network drivers.
    ESP_ERROR_CHECK(esp_netif_init());
    // Create the default event loop so Wi-Fi events can be delivered.
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize the LED hardware through the LED module.
    led_init();
    // Initialize Wi-Fi station mode through the Wi-Fi manager module.
    wifi_manager_init();

    // Main application loop: run forever after startup finishes.
    while (1) {
        // If Wi-Fi has a valid connection, blink the LED.
        if (wifi_manager_is_connected()) {
            led_toggle();
        } else {
            // If Wi-Fi is not connected, force the LED off.
            led_set(false);
        }

        // Wait before checking again and before the next blink toggle.
        vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD_MS));
    }
}
