#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#include <stdint.h>

// Valid ranges (setters snap to min + n*step and clamp to [min, max])
#define SETTINGS_SHOT_DOSE_MIN_MS      500U
#define SETTINGS_SHOT_DOSE_STEP_MS     500U
#define SETTINGS_SHOT_DOSE_MAX_MS      5000U
#define SETTINGS_MIX_INTERVAL_MIN_MS   3000U
#define SETTINGS_MIX_INTERVAL_STEP_MS  1000U
#define SETTINGS_MIX_INTERVAL_MAX_MS   30000U
#define SETTINGS_TARGET_EC_MIN         0.3f
#define SETTINGS_TARGET_EC_STEP        0.1f
#define SETTINGS_TARGET_EC_MAX         5.0f

// EC probe calibration factor (single-point vs 1413 µS/cm KCl reference)
// k = 1413 / EC_raw_mean; EC_true = k * EC_raw
#define SETTINGS_EC_CAL_FACTOR_MIN  0.7f
#define SETTINGS_EC_CAL_FACTOR_MAX  1.3f

// Loads persisted settings from NVS (also initializes NVS; call once early in app_main)
void settings_manager_init(void);

// Getters are safe from any task
uint32_t settings_get_shot_dose_ms(void);      // "Shot Dose" pump on-time
uint32_t settings_get_mix_interval_ms(void);   // "Target Dose" mixing wait between doses
float    settings_get_target_ec(void);         // last used target EC (mS/cm)
float    settings_get_ec_cal_factor(void);     // EC correction factor (1.0 = uncalibrated)

// Setters clamp/snap, update RAM immediately, and persist to NVS
void settings_set_shot_dose_ms(uint32_t ms);
void settings_set_mix_interval_ms(uint32_t ms);
void settings_set_target_ec(float ec);
void settings_set_ec_cal_factor(float k);      // clamp to [0.7, 1.3]
void settings_clear_ec_cal_factor(void);       // reset to 1.0 and erase NVS key

#endif // SETTINGS_MANAGER_H
