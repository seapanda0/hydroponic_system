#ifndef MQTT_CONFIG_H
#define MQTT_CONFIG_H

// ---------------------------------------------------------------------------
// Master switch — set to 0 to strip all MQTT code from the build
// ---------------------------------------------------------------------------
#define HYDRO_MQTT_ENABLED  1

// ---------------------------------------------------------------------------
// Broker connection
// ---------------------------------------------------------------------------
#define HYDRO_MQTT_BROKER_HOST  "192.168.31.242"
#define HYDRO_MQTT_BROKER_PORT  1883
#define HYDRO_MQTT_BROKER_URI   "mqtt://" HYDRO_MQTT_BROKER_HOST

#define HYDRO_MQTT_USERNAME     "mmu"
#define HYDRO_MQTT_PASSWORD     "smartfarm"
#define HYDRO_MQTT_CLIENT_ID    "hydro_esp32"

// Reconnect back-off cap (ms)
#define HYDRO_MQTT_RECONNECT_TIMEOUT_MS  5000

// ---------------------------------------------------------------------------
// Sensor topics  (published every HYDRO_MQTT_PUBLISH_INTERVAL_S seconds)
// ---------------------------------------------------------------------------
#define HYDRO_TOPIC_ROOT        "hydro"

#define HYDRO_TOPIC_TEMPERATURE "hydro/sensor/temperature"   // float  °C
#define HYDRO_TOPIC_HUMIDITY    "hydro/sensor/humidity"      // float  %
#define HYDRO_TOPIC_WATER_LEVEL "hydro/sensor/water_level"   // uint   %
#define HYDRO_TOPIC_DISTANCE    "hydro/sensor/distance"      // uint   mm (raw DYP-A02)
#define HYDRO_TOPIC_EC          "hydro/sensor/ec"            // uint   µS/cm (calibrated)
#define HYDRO_TOPIC_TDS         "hydro/sensor/tds"           // uint   PPM
#define HYDRO_TOPIC_LIQUID1     "hydro/sensor/liquid1"       // 0 = DRY, 1 = WET
#define HYDRO_TOPIC_LIQUID2     "hydro/sensor/liquid2"       // 0 = DRY, 1 = WET

// Combined JSON status (same payload as the web API)
#define HYDRO_TOPIC_STATUS_JSON "hydro/status"

// ---------------------------------------------------------------------------
// Device-state topics  (published on state change)
// ---------------------------------------------------------------------------
#define HYDRO_TOPIC_STATE_PUMP   "hydro/state/pump"          // ON / OFF
#define HYDRO_TOPIC_STATE_LIGHT  "hydro/state/light"         // ON / OFF
#define HYDRO_TOPIC_STATE_FERT_A "hydro/state/fert_a"        // ON / OFF
#define HYDRO_TOPIC_STATE_FERT_B "hydro/state/fert_b"        // ON / OFF

// ---------------------------------------------------------------------------
// Sensor publish interval
// Sensor loop runs every 2 s; set this to how often MQTT should publish.
// ---------------------------------------------------------------------------
#define HYDRO_MQTT_PUBLISH_INTERVAL_S  120   // 2 minutes

// ---------------------------------------------------------------------------
// Publish quality-of-service and retain flag
// ---------------------------------------------------------------------------
#define HYDRO_MQTT_QOS     0    // fire-and-forget; increase to 1 if broker acks matter
#define HYDRO_MQTT_RETAIN  1    // broker stores last value for late subscribers

#endif // MQTT_CONFIG_H
