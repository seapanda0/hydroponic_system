#ifndef KINETIC_OS_H
#define KINETIC_OS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

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
void kinetic_os_set_wifi_name(const char * ssid);

// Event hooks for broadcasting UI taps back to ESP
typedef void (*kinetic_os_switch_cb_t)(bool is_on);
void kinetic_os_set_pump_switch_cb(kinetic_os_switch_cb_t cb);
void kinetic_os_set_light_switch_cb(kinetic_os_switch_cb_t cb);

void kinetic_os_set_light_intensity(uint8_t pct);
typedef void (*kinetic_os_slider_cb_t)(uint8_t pct);
void kinetic_os_set_light_intensity_cb(kinetic_os_slider_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif
