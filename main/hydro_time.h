#ifndef HYDRO_TIME_H
#define HYDRO_TIME_H

#include <stdint.h>
#include <stdbool.h>

// Local wall-clock offset from UTC applied when converting an NTP-synced
// epoch into hours/minutes. Edit for your timezone, e.g. 480 for UTC+8,
// -300 for UTC-5.
#define HYDRO_TIME_TZ_OFFSET_MIN 0

typedef enum {
    HYDRO_TIME_SOURCE_NONE = 0,
    HYDRO_TIME_SOURCE_MANUAL,
    HYDRO_TIME_SOURCE_NTP,
} hydro_time_source_t;

typedef enum {
    HYDRO_TIME_SYNC_IDLE = 0,
    HYDRO_TIME_SYNC_IN_PROGRESS,
    HYDRO_TIME_SYNC_SUCCESS,
    HYDRO_TIME_SYNC_FAIL_NO_WIFI,
    HYDRO_TIME_SYNC_FAIL_TIMEOUT,
} hydro_time_sync_status_t;

// Call once at boot, before anything else touches time. Restores the time
// reference and light-timer settings from RTC slow memory if they survived
// a warm reboot (esp_restart()/panic/watchdog reset); resets to defaults on
// a cold power-on. Never touches flash, so this can be called/updated as
// often as needed with zero wear cost.
//
// Caveat: RTC slow memory is only powered by VDD_RTC, which is NOT battery
// backed on this board (no dedicated RTC chip on V2). A full power loss or
// the EN/reset button (which behaves like a power-on reset on most boards)
// will still wipe this state. That's a hardware limitation, not a software
// one -- an external battery-backed RTC (e.g. DS3231) would be needed to
// survive that.
void hydro_time_init(void);

// True once we have any time reference (manual or NTP); false right after a
// cold boot until the user sets one.
bool hydro_time_is_set(void);

hydro_time_source_t hydro_time_get_source(void);

// Current wall-clock time of day (0-23 / 0-59 / 0-59). Reads as 00:00:00 if
// hydro_time_is_set() is false.
void hydro_time_get_current(uint8_t *hour, uint8_t *minute, uint8_t *second);

// Sets the time reference directly (RTC slow memory only, no flash write).
void hydro_time_set_manual(uint8_t hour, uint8_t minute);

// Blocking NTP sync (up to ~5s). Only call from a dedicated task, never from
// the display task -- it must stay free to run LVGL. Returns the final
// status; also updates the value returned by hydro_time_get_sync_status().
hydro_time_sync_status_t hydro_time_sync_ntp(void);

// Last known sync attempt status/result (safe to poll from the display task).
hydro_time_sync_status_t hydro_time_get_sync_status(void);

// --- Grow light timer ---
bool hydro_time_get_timer_enabled(void);
void hydro_time_set_timer_enabled(bool enabled);

void hydro_time_get_on_time(uint8_t *hour, uint8_t *minute);
void hydro_time_get_off_time(uint8_t *hour, uint8_t *minute);
void hydro_time_set_on_time(uint8_t hour, uint8_t minute);
void hydro_time_set_off_time(uint8_t hour, uint8_t minute);

// True if the current time-of-day falls within [on_time, off_time), handling
// schedules that cross midnight. Always false if hydro_time_is_set() is
// false or the timer is disabled.
bool hydro_time_light_should_be_on(void);

#endif
