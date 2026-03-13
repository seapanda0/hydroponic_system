#include "hydro_mqtt.h"

#if HYDRO_MQTT_ENABLED

#include "mqtt_config.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "HYDRO_MQTT";

static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool            s_connected = false;

// ---------------------------------------------------------------------------
// Event handler
// ---------------------------------------------------------------------------

static void mqtt_event_handler(void *arg, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {

    case MQTT_EVENT_CONNECTED:
        s_connected = true;
        ESP_LOGI(TAG, "Connected to broker %s", HYDRO_MQTT_BROKER_URI);
        break;

    case MQTT_EVENT_DISCONNECTED:
        s_connected = false;
        ESP_LOGW(TAG, "Disconnected — will retry automatically");
        break;

    case MQTT_EVENT_ERROR:
        s_connected = false;
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "TCP error: 0x%x  errno %d",
                     event->error_handle->esp_tls_last_esp_err,
                     event->error_handle->esp_transport_sock_errno);
        } else {
            ESP_LOGE(TAG, "MQTT error type: %d", event->error_handle->error_type);
        }
        break;

    case MQTT_EVENT_PUBLISHED:
        ESP_LOGD(TAG, "msg_id=%d published", event->msg_id);
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void hydro_mqtt_init(void)
{
    esp_mqtt_client_config_t cfg = {
        .broker = {
            .address.uri = HYDRO_MQTT_BROKER_URI,
        },
        .credentials = {
            .username        = HYDRO_MQTT_USERNAME,
            .client_id       = HYDRO_MQTT_CLIENT_ID,
            .authentication.password = HYDRO_MQTT_PASSWORD,
        },
        .network = {
            .reconnect_timeout_ms = HYDRO_MQTT_RECONNECT_TIMEOUT_MS,
        },
    };

    s_client = esp_mqtt_client_init(&cfg);
    if (!s_client) {
        ESP_LOGE(TAG, "Failed to create MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID,
                                   mqtt_event_handler, NULL);
    esp_err_t err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_mqtt_client_start: %s", esp_err_to_name(err));
    }
}

bool hydro_mqtt_is_connected(void)
{
    return s_connected;
}

// Thin wrapper — skips silently if not yet connected.
static void publish(const char *topic, const char *payload)
{
    if (!s_client || !s_connected) return;
    int id = esp_mqtt_client_publish(s_client, topic, payload,
                                     0 /* infer from strlen */,
                                     HYDRO_MQTT_QOS, HYDRO_MQTT_RETAIN);
    if (id < 0) {
        ESP_LOGW(TAG, "publish failed: %s = %s", topic, payload);
    }
}

void hydro_mqtt_publish_sensors(float    temp_c,
                                float    humidity_pct,
                                uint8_t  water_level_pct,
                                uint16_t distance_mm,
                                uint32_t ec_us_cm,
                                uint16_t tds_ppm,
                                bool     liquid1_wet,
                                bool     liquid2_wet)
{
    if (!s_client || !s_connected) return;

    char buf[32];

    snprintf(buf, sizeof(buf), "%.2f", (double)temp_c);
    publish(HYDRO_TOPIC_TEMPERATURE, buf);

    snprintf(buf, sizeof(buf), "%.2f", (double)humidity_pct);
    publish(HYDRO_TOPIC_HUMIDITY, buf);

    snprintf(buf, sizeof(buf), "%u", (unsigned)water_level_pct);
    publish(HYDRO_TOPIC_WATER_LEVEL, buf);

    snprintf(buf, sizeof(buf), "%u", (unsigned)distance_mm);
    publish(HYDRO_TOPIC_DISTANCE, buf);

    snprintf(buf, sizeof(buf), "%lu", (unsigned long)ec_us_cm);
    publish(HYDRO_TOPIC_EC, buf);

    snprintf(buf, sizeof(buf), "%u", (unsigned)tds_ppm);
    publish(HYDRO_TOPIC_TDS, buf);

    publish(HYDRO_TOPIC_LIQUID1, liquid1_wet ? "1" : "0");
    publish(HYDRO_TOPIC_LIQUID2, liquid2_wet ? "1" : "0");

    // Combined JSON blob
    char json[192];
    snprintf(json, sizeof(json),
             "{\"temperature\":%.2f,\"humidity\":%.2f,"
             "\"water_level\":%u,\"distance\":%u,"
             "\"ec\":%lu,\"tds\":%u,"
             "\"liquid1\":%s,\"liquid2\":%s}",
             (double)temp_c, (double)humidity_pct,
             (unsigned)water_level_pct, (unsigned)distance_mm,
             (unsigned long)ec_us_cm, (unsigned)tds_ppm,
             liquid1_wet ? "true" : "false",
             liquid2_wet ? "true" : "false");
    publish(HYDRO_TOPIC_STATUS_JSON, json);
}

void hydro_mqtt_publish_device_state(const char *topic, bool on)
{
    publish(topic, on ? "ON" : "OFF");
}

#endif // HYDRO_MQTT_ENABLED
