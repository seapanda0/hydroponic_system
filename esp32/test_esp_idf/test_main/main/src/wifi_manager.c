// Include this module's public declarations first.
#include "wifi_manager.h"

// Needed for strlen() and strlcpy().
#include <string.h>

// Provides ESP_ERROR_CHECK().
#include "esp_err.h"
// Lets us register and receive Wi-Fi and IP events.
#include "esp_event.h"
// Provides ESP_LOGI/W/E logging macros.
#include "esp_log.h"
// Provides the default network interface for station mode.
#include "esp_netif.h"
// Main ESP-IDF Wi-Fi driver API.
#include "esp_wifi.h"
// Exposes CONFIG_WIFI_SSID and related project settings.
#include "sdkconfig.h"

// Copy menuconfig Wi-Fi values into short local macro names.
#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASSWORD
#define WIFI_MAXIMUM_RETRY CONFIG_WIFI_MAXIMUM_RETRY

// Logging tag so serial output shows which module produced the message.
static const char *TAG = "wifi_manager";

// Shared connection flag read by main.c and updated by Wi-Fi events.
static volatile bool s_wifi_connected = false;
// Counts how many reconnect attempts have happened after a disconnect.
static int s_retry_count = 0;

// This callback runs whenever subscribed Wi-Fi or IP events occur.
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    // We do not need the user-supplied argument in this module.
    (void)arg;

    // When the Wi-Fi driver starts, begin the first connection attempt.
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wi-Fi station started, connecting to \"%s\"", WIFI_SSID);
        ESP_ERROR_CHECK(esp_wifi_connect());
        return;
    }

    // If the station disconnects, mark the link as down and optionally retry.
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;

        // Retry only up to the configured limit.
        if (s_retry_count < WIFI_MAXIMUM_RETRY) {
            // Count this retry attempt.
            s_retry_count++;
            ESP_LOGW(TAG, "Wi-Fi disconnected, retry %d/%d", s_retry_count, WIFI_MAXIMUM_RETRY);
            // Ask the Wi-Fi driver to connect again.
            ESP_ERROR_CHECK(esp_wifi_connect());
        } else {
            // Stop retrying and report the failure.
            ESP_LOGE(TAG, "Wi-Fi connection failed after %d retries", WIFI_MAXIMUM_RETRY);
        }
        return;
    }

    // This event means the station connected successfully and received an IP address.
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        // Cast the generic event data to the correct IP event type.
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;

        // Mark the Wi-Fi link as usable.
        s_wifi_connected = true;
        // Reset retry count after a successful connection.
        s_retry_count = 0;

        // Print the assigned IP address to the serial console.
        ESP_LOGI(TAG, "Connected with IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

// Initialize the Wi-Fi driver in station mode.
void wifi_manager_init(void)
{
    // Create the default network interface object for station mode.
    esp_netif_create_default_wifi_sta();

    // Fill the Wi-Fi driver config with ESP-IDF defaults.
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Start the Wi-Fi driver with that config.
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register for Wi-Fi state-change events.
    ESP_ERROR_CHECK(esp_event_handler_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    // Register for the event that reports the received IP address.
    ESP_ERROR_CHECK(esp_event_handler_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    // Create and zero-initialize the station configuration structure.
    wifi_config_t wifi_config = {
        .sta = {
            // Accept WPA2 or stronger by default.
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    // Copy the configured SSID into the fixed-size driver buffer safely.
    strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    // Copy the configured password into the fixed-size driver buffer safely.
    strlcpy((char *)wifi_config.sta.password, WIFI_PASS, sizeof(wifi_config.sta.password));

    // If the password is empty, allow connecting to an open network.
    if (strlen(WIFI_PASS) == 0) {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }

    // Put the Wi-Fi peripheral into station-only mode.
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    // Apply the SSID/password configuration to the station interface.
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    // Start the Wi-Fi driver; this will later trigger WIFI_EVENT_STA_START.
    ESP_ERROR_CHECK(esp_wifi_start());
}

// Let other modules ask whether Wi-Fi is currently connected.
bool wifi_manager_is_connected(void)
{
    // Return the latest connection flag updated by the event handler.
    return s_wifi_connected;
}
