#include "hydro_time.h"

#include <string.h>
#include <sys/time.h>

#include "esp_attr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"

#include "web_server.h"  // wifi_is_connected()

static const char *TAG = "HYDRO_TIME";

#define HYDRO_TIME_MAGIC 0x484D5231u // "HMR1"

typedef struct {
    uint32_t magic;
    int32_t  ref_seconds_since_midnight;
    int64_t  ref_esp_timer_us;
    uint8_t  source; // hydro_time_source_t
    bool     timer_enabled;
    uint16_t on_minutes;
    uint16_t off_minutes;
} hydro_time_state_t;

// RTC slow memory: survives esp_restart()/panic/watchdog resets with zero
// flash writes, but is NOT preserved across a full power loss or the
// EN/reset button (both behave like a power-on reset for the RTC domain on
// this board, which has no battery-backed RTC chip).
static RTC_NOINIT_ATTR hydro_time_state_t s_state;

static volatile hydro_time_sync_status_t s_sync_status = HYDRO_TIME_SYNC_IDLE;
static bool s_sntp_started = false;

void hydro_time_init(void)
{
    if (s_state.magic != HYDRO_TIME_MAGIC) {
        ESP_LOGI(TAG, "Cold boot: initializing time state to defaults");
        memset(&s_state, 0, sizeof(s_state));
        s_state.magic = HYDRO_TIME_MAGIC;
        s_state.source = HYDRO_TIME_SOURCE_NONE;
    } else {
        uint8_t h, m, s;
        hydro_time_get_current(&h, &m, &s);
        ESP_LOGI(TAG, "Restored time state across reboot: %02u:%02u:%02u (source=%u, timer=%s)",
                 h, m, s, (unsigned)s_state.source, s_state.timer_enabled ? "on" : "off");
    }
}

bool hydro_time_is_set(void)
{
    return s_state.source != HYDRO_TIME_SOURCE_NONE;
}

hydro_time_source_t hydro_time_get_source(void)
{
    return (hydro_time_source_t)s_state.source;
}

static void set_reference(int32_t seconds_since_midnight, hydro_time_source_t source)
{
    seconds_since_midnight = ((seconds_since_midnight % 86400) + 86400) % 86400;
    s_state.ref_seconds_since_midnight = seconds_since_midnight;
    s_state.ref_esp_timer_us = esp_timer_get_time();
    s_state.source = (uint8_t)source;
}

void hydro_time_get_current(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if (s_state.source == HYDRO_TIME_SOURCE_NONE) {
        if (hour) *hour = 0;
        if (minute) *minute = 0;
        if (second) *second = 0;
        return;
    }
    int64_t elapsed_s = (esp_timer_get_time() - s_state.ref_esp_timer_us) / 1000000;
    int32_t now = (int32_t)(((int64_t)s_state.ref_seconds_since_midnight + elapsed_s) % 86400);
    if (now < 0) now += 86400;
    if (hour) *hour = (uint8_t)(now / 3600);
    if (minute) *minute = (uint8_t)((now % 3600) / 60);
    if (second) *second = (uint8_t)(now % 60);
}

void hydro_time_set_manual(uint8_t hour, uint8_t minute)
{
    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    set_reference((int32_t)hour * 3600 + (int32_t)minute * 60, HYDRO_TIME_SOURCE_MANUAL);
    ESP_LOGI(TAG, "Manual time set: %02u:%02u", hour, minute);
}

hydro_time_sync_status_t hydro_time_sync_ntp(void)
{
    if (!wifi_is_connected()) {
        s_sync_status = HYDRO_TIME_SYNC_FAIL_NO_WIFI;
        return s_sync_status;
    }

    s_sync_status = HYDRO_TIME_SYNC_IN_PROGRESS;

    if (!s_sntp_started) {
        esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
        config.start = false;
        config.wait_for_sync = false;
        esp_netif_sntp_init(&config);
        s_sntp_started = true;
    }
    esp_netif_sntp_start();

    esp_err_t err = esp_netif_sntp_sync_wait(pdMS_TO_TICKS(5000));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NTP sync timed out: %s", esp_err_to_name(err));
        s_sync_status = HYDRO_TIME_SYNC_FAIL_TIMEOUT;
        return s_sync_status;
    }

    struct timeval tv;
    gettimeofday(&tv, NULL);
    int32_t local_seconds = (int32_t)((tv.tv_sec + (int64_t)HYDRO_TIME_TZ_OFFSET_MIN * 60) % 86400);
    if (local_seconds < 0) local_seconds += 86400;
    set_reference(local_seconds, HYDRO_TIME_SOURCE_NTP);

    ESP_LOGI(TAG, "NTP sync OK, local time now %02u:%02u",
             (unsigned)(local_seconds / 3600), (unsigned)((local_seconds % 3600) / 60));
    s_sync_status = HYDRO_TIME_SYNC_SUCCESS;
    return s_sync_status;
}

hydro_time_sync_status_t hydro_time_get_sync_status(void)
{
    return s_sync_status;
}

bool hydro_time_get_timer_enabled(void)
{
    return s_state.timer_enabled;
}

void hydro_time_set_timer_enabled(bool enabled)
{
    s_state.timer_enabled = enabled;
}

void hydro_time_get_on_time(uint8_t *hour, uint8_t *minute)
{
    if (hour) *hour = (uint8_t)(s_state.on_minutes / 60U);
    if (minute) *minute = (uint8_t)(s_state.on_minutes % 60U);
}

void hydro_time_get_off_time(uint8_t *hour, uint8_t *minute)
{
    if (hour) *hour = (uint8_t)(s_state.off_minutes / 60U);
    if (minute) *minute = (uint8_t)(s_state.off_minutes % 60U);
}

void hydro_time_set_on_time(uint8_t hour, uint8_t minute)
{
    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    s_state.on_minutes = (uint16_t)hour * 60U + minute;
}

void hydro_time_set_off_time(uint8_t hour, uint8_t minute)
{
    if (hour > 23) hour = 23;
    if (minute > 59) minute = 59;
    s_state.off_minutes = (uint16_t)hour * 60U + minute;
}

bool hydro_time_light_should_be_on(void)
{
    if (s_state.source == HYDRO_TIME_SOURCE_NONE || !s_state.timer_enabled) {
        return false;
    }
    uint8_t h, m, s;
    hydro_time_get_current(&h, &m, &s);
    uint16_t now_min = (uint16_t)h * 60U + m;
    uint16_t on = s_state.on_minutes;
    uint16_t off = s_state.off_minutes;
    if (on == off) {
        return false; // zero-length window: never on
    }
    if (on < off) {
        return now_min >= on && now_min < off;
    }
    return now_min >= on || now_min < off; // window crosses midnight
}
