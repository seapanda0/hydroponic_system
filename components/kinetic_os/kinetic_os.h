#ifndef KINETIC_OS_H
#define KINETIC_OS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

// Theme Colors
#define KINETIC_COLOR_BG lv_color_hex(0x0d0f0d)
#define KINETIC_COLOR_SURFACE lv_color_hex(0x121412)
#define KINETIC_COLOR_SURFACE_VARIANT lv_color_hex(0x242623)
#define KINETIC_COLOR_PRIMARY lv_color_hex(0x8eff71)
#define KINETIC_COLOR_PRIMARY_DIM lv_color_hex(0x2be800)
#define KINETIC_COLOR_ON_PRIMARY lv_color_hex(0x064200)
#define KINETIC_COLOR_SECONDARY lv_color_hex(0xafefdd)
#define KINETIC_COLOR_TERTIARY lv_color_hex(0x45fec9)
#define KINETIC_COLOR_TEXT lv_color_white()
#define KINETIC_COLOR_TEXT_DIM lv_color_white()
#define KINETIC_COLOR_OUTLINE lv_color_hex(0x474846)

// Expose the global initialization
void kinetic_os_ui_init(void);

// Hardware Synchronization API for ESP Dashboard
void kinetic_os_set_pump_state(bool on);
void kinetic_os_set_light_state(bool on);
void kinetic_os_set_fertilizer_a_state(bool on);
void kinetic_os_set_fertilizer_b_state(bool on);
void kinetic_os_set_wifi_name(const char * ssid);
void kinetic_os_set_temperature(float celsius);
void kinetic_os_set_humidity(float pct);
void kinetic_os_set_water_level(uint8_t pct);

// Water chemistry display API
void kinetic_os_set_tds(uint16_t ppm);
void kinetic_os_set_ec(float ms_per_cm);

// Event hooks for broadcasting UI taps back to ESP
typedef void (*kinetic_os_switch_cb_t)(bool is_on);
void kinetic_os_set_pump_switch_cb(kinetic_os_switch_cb_t cb);
void kinetic_os_set_light_switch_cb(kinetic_os_switch_cb_t cb);
void kinetic_os_set_fertilizer_a_switch_cb(kinetic_os_switch_cb_t cb);
void kinetic_os_set_fertilizer_b_switch_cb(kinetic_os_switch_cb_t cb);

void kinetic_os_set_light_intensity(uint8_t pct);
typedef void (*kinetic_os_slider_cb_t)(uint8_t pct);

#ifdef __cplusplus
}
#endif

#endif
