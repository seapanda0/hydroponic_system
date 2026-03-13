# WiFi LED Module Split Design

## Goal

Refactor the current single-file `main.c` implementation into small feature modules while preserving the current runtime behavior: the ESP32 connects in station mode and the LED blinks only after the device gets an IP address.

## Architecture

`main.c` remains the application entry point and orchestration layer. It initializes system services, starts the LED and Wi-Fi modules, and runs the top-level blink loop.

`wifi_manager.c` owns station initialization, event registration, reconnect handling, and the internal connection-state flag. `led.c` owns LED GPIO initialization and output state changes. Public headers expose only the minimum APIs needed by `main.c`.

## Components

- `main/src/main.c`
  - Initialize NVS, netif, and event loop
  - Initialize LED and Wi-Fi modules
  - Blink only when Wi-Fi reports connected
- `main/src/wifi_manager.c`
  - Configure STA mode from Kconfig
  - Register Wi-Fi and IP event handlers
  - Track connected/disconnected state
  - Retry reconnects up to configured limit
- `main/src/led.c`
  - Configure GPIO as output
  - Provide `led_init`, `led_set`, and `led_toggle`
- `main/include/wifi_manager.h`
  - Public Wi-Fi API
- `main/include/led.h`
  - Public LED API

## Behavior

Behavior remains unchanged from the current implementation:

- On boot, initialize NVS, TCP/IP stack, default event loop, LED, and Wi-Fi STA.
- When Wi-Fi starts, attempt to connect to the configured AP.
- When disconnected, clear the connection flag, force the LED off, and retry up to the configured limit.
- When an IP address is obtained, mark Wi-Fi as connected.
- In the main loop, toggle the LED every `CONFIG_BLINK_PERIOD` milliseconds only while connected.

## Error Handling

- Continue using `ESP_ERROR_CHECK` for initialization and driver calls.
- Handle NVS recovery for `ESP_ERR_NVS_NO_FREE_PAGES` and `ESP_ERR_NVS_NEW_VERSION_FOUND`.
- Keep Wi-Fi event callback internal to `wifi_manager.c`.

## Verification

- Confirm only one `app_main()` is compiled into the build target.
- Check that `main/CMakeLists.txt` includes the new source files.
- Build with `idf.py build` in a correctly configured ESP-IDF shell.
- Flash and confirm LED behavior before and after Wi-Fi connection.
