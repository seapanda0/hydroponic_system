// Include this module's public declarations first.
#include "led.h"

// GPIO driver controls the LED pin.
#include "driver/gpio.h"
// Provides ESP_ERROR_CHECK() for defensive error handling.
#include "esp_err.h"
// Gives access to CONFIG_BLINK_GPIO from menuconfig.
#include "sdkconfig.h"

// Use the configured LED pin number from project configuration.
#define BLINK_GPIO CONFIG_BLINK_GPIO

// Keep track of the current LED state inside this module only.
static bool s_led_state = false;

// Configure the LED pin once during startup.
void led_init(void)
{
    // Reset the pin to a clean default state before reconfiguring it.
    gpio_reset_pin(BLINK_GPIO);
    // Set the pin as a digital output so we can drive the LED.
    ESP_ERROR_CHECK(gpio_set_direction(BLINK_GPIO, GPIO_MODE_OUTPUT));
    // Start with the LED turned off.
    led_set(false);
}

// Turn the LED on or off.
void led_set(bool on)
{
    // Save the new state so led_toggle() knows what to invert later.
    s_led_state = on;
    // Write a logic level to the pin: 1 for on, 0 for off.
    ESP_ERROR_CHECK(gpio_set_level(BLINK_GPIO, on ? 1 : 0));
}

// Flip the LED from its current state to the opposite state.
void led_toggle(void)
{
    // Reuse led_set() so state tracking stays in one place.
    led_set(!s_led_state);
}
