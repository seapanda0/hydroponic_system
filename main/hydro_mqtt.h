#ifndef HYDRO_MQTT_H
#define HYDRO_MQTT_H

#include "mqtt_config.h"
#include <stdint.h>
#include <stdbool.h>

#if HYDRO_MQTT_ENABLED

// Call once after wifi_init_sta() in app_main.
void hydro_mqtt_init(void);

// Publish all sensor readings.  Call from sensor_read_task (~every 2 s).
// Also publishes a combined JSON blob to HYDRO_TOPIC_STATUS_JSON.
void hydro_mqtt_publish_sensors(float    temp_c,
                                float    humidity_pct,
                                uint8_t  water_level_pct,
                                uint16_t distance_mm,
                                uint32_t ec_us_cm,
                                uint16_t tds_ppm,
                                bool     liquid1_wet,
                                bool     liquid2_wet);

// Publish a device state change (ON / OFF).  Call from toggle callbacks.
void hydro_mqtt_publish_device_state(const char *topic, bool on);

// Returns true once the client has connected at least once.
bool hydro_mqtt_is_connected(void);

#else // HYDRO_MQTT_ENABLED == 0 — compile everything away

static inline void hydro_mqtt_init(void)                                {}
static inline void hydro_mqtt_publish_sensors(float a, float b,
    uint8_t c, uint16_t d, uint32_t e, uint16_t f, bool g, bool h)    { (void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h; }
static inline void hydro_mqtt_publish_device_state(const char *t, bool on) { (void)t;(void)on; }
static inline bool hydro_mqtt_is_connected(void)                        { return false; }

#endif // HYDRO_MQTT_ENABLED

#endif // HYDRO_MQTT_H
