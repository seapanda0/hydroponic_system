#include "settings_manager.h"

#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "SETTINGS";

#define SETTINGS_NVS_NAMESPACE  "settings"
#define SETTINGS_KEY_SHOT_MS    "shot_ms"
#define SETTINGS_KEY_MIX_MS     "mix_ms"
#define SETTINGS_KEY_EC_TENTHS  "ec_tenths"
#define SETTINGS_KEY_EC_CAL_K   "ec_cal_k"   // stored as uint32_t = k * 1000 (e.g. 1.021 -> 1021)

// RAM copies. 32-bit reads/writes are atomic on ESP32, so getters are safe
// from any task or esp_timer callback without locking.
static uint32_t s_shot_dose_ms = SETTINGS_SHOT_DOSE_MIN_MS;    // default 500 ms
static uint32_t s_mix_interval_ms = SETTINGS_MIX_INTERVAL_MIN_MS; // default 3000 ms
static uint32_t s_target_ec_tenths = 10;                       // default 1.0 mS/cm
static float    s_ec_cal_factor = 1.0f;                        // default: uncalibrated

// Snaps a value to min + n*step (rounding to the nearest step) and clamps to [min, max]
static uint32_t snap_u32(uint32_t value, uint32_t min, uint32_t step, uint32_t max)
{
    if (value <= min) {
        return min;
    }
    uint32_t steps = (value - min + step / 2) / step;
    uint32_t snapped = min + steps * step;
    if (snapped > max) {
        snapped = max;
    }
    return snapped;
}

static void nvs_store_u32(const char *key, uint32_t value)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed for %s: %s", key, esp_err_to_name(err));
        return;
    }

    err = nvs_set_u32(handle, key, value);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_set_u32 failed for %s: %s", key, esp_err_to_name(err));
    } else {
        err = nvs_commit(handle);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "nvs_commit failed for %s: %s", key, esp_err_to_name(err));
        }
    }
    nvs_close(handle);
}

void settings_manager_init(void)
{
    // Initialize NVS (idempotent; wifi_init_sta calling it again later is harmless)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err == ESP_OK) {
        // Missing keys keep their defaults
        nvs_get_u32(handle, SETTINGS_KEY_SHOT_MS, &s_shot_dose_ms);
        nvs_get_u32(handle, SETTINGS_KEY_MIX_MS, &s_mix_interval_ms);
        nvs_get_u32(handle, SETTINGS_KEY_EC_TENTHS, &s_target_ec_tenths);
        uint32_t cal_k_stored = 0;
        if (nvs_get_u32(handle, SETTINGS_KEY_EC_CAL_K, &cal_k_stored) == ESP_OK && cal_k_stored > 0) {
            float k = (float)cal_k_stored / 1000.0f;
            if (k >= SETTINGS_EC_CAL_FACTOR_MIN && k <= SETTINGS_EC_CAL_FACTOR_MAX) {
                s_ec_cal_factor = k;
            }
        }
    } else if (err != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "nvs_open failed: %s", esp_err_to_name(err));
    }
    if (err == ESP_OK) {
        nvs_close(handle);
    }

    // Sanitize whatever came out of flash back onto the valid grid
    s_shot_dose_ms = snap_u32(s_shot_dose_ms,
                              SETTINGS_SHOT_DOSE_MIN_MS,
                              SETTINGS_SHOT_DOSE_STEP_MS,
                              SETTINGS_SHOT_DOSE_MAX_MS);
    s_mix_interval_ms = snap_u32(s_mix_interval_ms,
                                 SETTINGS_MIX_INTERVAL_MIN_MS,
                                 SETTINGS_MIX_INTERVAL_STEP_MS,
                                 SETTINGS_MIX_INTERVAL_MAX_MS);
    s_target_ec_tenths = snap_u32(s_target_ec_tenths,
                                  (uint32_t)(SETTINGS_TARGET_EC_MIN * 10.0f + 0.5f),
                                  (uint32_t)(SETTINGS_TARGET_EC_STEP * 10.0f + 0.5f),
                                  (uint32_t)(SETTINGS_TARGET_EC_MAX * 10.0f + 0.5f));

    ESP_LOGI(TAG, "Loaded: shot dose %u ms, mix interval %u ms, target EC %u.%u mS/cm, cal k=%.3f",
             (unsigned)s_shot_dose_ms,
             (unsigned)s_mix_interval_ms,
             (unsigned)(s_target_ec_tenths / 10U),
             (unsigned)(s_target_ec_tenths % 10U),
             (double)s_ec_cal_factor);
}

float settings_get_ec_cal_factor(void)
{
    return s_ec_cal_factor;
}

void settings_set_ec_cal_factor(float k)
{
    if (k < SETTINGS_EC_CAL_FACTOR_MIN) k = SETTINGS_EC_CAL_FACTOR_MIN;
    if (k > SETTINGS_EC_CAL_FACTOR_MAX) k = SETTINGS_EC_CAL_FACTOR_MAX;
    s_ec_cal_factor = k;
    nvs_store_u32(SETTINGS_KEY_EC_CAL_K, (uint32_t)(k * 1000.0f + 0.5f));
}

void settings_clear_ec_cal_factor(void)
{
    s_ec_cal_factor = 1.0f;
    nvs_handle_t handle;
    esp_err_t err = nvs_open(SETTINGS_NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs_open failed clearing ec_cal_k: %s", esp_err_to_name(err));
        return;
    }
    nvs_erase_key(handle, SETTINGS_KEY_EC_CAL_K);
    nvs_commit(handle);
    nvs_close(handle);
    ESP_LOGI(TAG, "EC calibration cleared (k=1.0)");
}

uint32_t settings_get_shot_dose_ms(void)
{
    return s_shot_dose_ms;
}

uint32_t settings_get_mix_interval_ms(void)
{
    return s_mix_interval_ms;
}

float settings_get_target_ec(void)
{
    return (float)s_target_ec_tenths / 10.0f;
}

void settings_set_shot_dose_ms(uint32_t ms)
{
    s_shot_dose_ms = snap_u32(ms,
                              SETTINGS_SHOT_DOSE_MIN_MS,
                              SETTINGS_SHOT_DOSE_STEP_MS,
                              SETTINGS_SHOT_DOSE_MAX_MS);
    nvs_store_u32(SETTINGS_KEY_SHOT_MS, s_shot_dose_ms);
}

void settings_set_mix_interval_ms(uint32_t ms)
{
    s_mix_interval_ms = snap_u32(ms,
                                 SETTINGS_MIX_INTERVAL_MIN_MS,
                                 SETTINGS_MIX_INTERVAL_STEP_MS,
                                 SETTINGS_MIX_INTERVAL_MAX_MS);
    nvs_store_u32(SETTINGS_KEY_MIX_MS, s_mix_interval_ms);
}

void settings_set_target_ec(float ec)
{
    if (ec < 0.0f) {
        ec = 0.0f;
    }
    uint32_t tenths = (uint32_t)(ec * 10.0f + 0.5f);
    s_target_ec_tenths = snap_u32(tenths,
                                  (uint32_t)(SETTINGS_TARGET_EC_MIN * 10.0f + 0.5f),
                                  (uint32_t)(SETTINGS_TARGET_EC_STEP * 10.0f + 0.5f),
                                  (uint32_t)(SETTINGS_TARGET_EC_MAX * 10.0f + 0.5f));
    nvs_store_u32(SETTINGS_KEY_EC_TENTHS, s_target_ec_tenths);
}
