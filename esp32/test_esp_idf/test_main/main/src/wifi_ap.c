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

void wifi_ap_init()