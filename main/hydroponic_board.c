#include "pinout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "esp_timer.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "st7789.h"
#include "sht4x.h"
#include "ultrasonic_sensor.h"
#include "lvgl.h"
#include "kinetic_os.h"
#include "ba234.h"

#include "wifi.h"        // WiFi credentials (SSID also shown on the display)
#include "web_server.h"  // WiFi + HTTP server module
#include "settings_manager.h"  // NVS-backed dosing settings
#include "hydro_mqtt.h"  // MQTT client (enable/disable via mqtt_config.h)
#include "hydro_time.h"  // Virtual RTC + grow light schedule

// Dosing head state. Each head has a dedicated pump GPIO and valve GPIO.
// On V1 both point to the same pin (wired in parallel); on V2 they are separate.
typedef struct {
    gpio_num_t pump_gpio;                   // output driving the pump
    gpio_num_t valve_gpio;                  // output driving the valve
    gpio_num_t liquid_sensor;               // liquid sensor watching this head's line
    const char *tag;                        // log tag / readable name
    volatile bool prime_active;             // "Prime Line" routine running
    volatile bool shot_dose_active;         // "Shot Dose" routine running
    esp_timer_handle_t prime_sample_timer;  // periodic liquid sensor sampling during prime
    uint16_t prime_sample_count;
    uint16_t prime_high_count;
    esp_timer_handle_t shot_dose_timer;     // one-shot timer turning the output back off
} dosing_head_t;

static dosing_head_t s_dosing_heads[2] = {
    {
        .pump_gpio    = FERT_A_PUMP_GPIO,
        .valve_gpio   = FERT_A_VALVE_GPIO,
        .liquid_sensor = LIQUID_SENSOR_A,
        .tag = "Dosing Head A",
    },
    {
        .pump_gpio    = FERT_B_PUMP_GPIO,
        .valve_gpio   = FERT_B_VALVE_GPIO,
        .liquid_sensor = LIQUID_SENSOR_B,
        .tag = "Dosing Head B",
    },
};

// A dose group is the set of heads a "Target Dose" routine drives together.
typedef struct {
    const char *tag;                        // log tag / readable name
    dosing_head_t *heads[2];                // heads dosed by this group
    size_t head_count;
    volatile bool active;                   // "Target Dose" routine running for this group
    float target_ec;                        // target EC setpoint (mS/cm)
} dose_group_t;

enum {
    DOSE_GROUP_A,
    DOSE_GROUP_B,
    DOSE_GROUP_AB,
    DOSE_GROUP_COUNT
};

static dose_group_t s_dose_groups[DOSE_GROUP_COUNT] = {
    [DOSE_GROUP_A] = {
        .tag = "Target Dose A",
        .heads = { &s_dosing_heads[0] },
        .head_count = 1,
        .target_ec = 1.0f,
    },
    [DOSE_GROUP_B] = {
        .tag = "Target Dose B",
        .heads = { &s_dosing_heads[1] },
        .head_count = 1,
        .target_ec = 1.0f,
    },
    [DOSE_GROUP_AB] = {
        .tag = "Target Dose A+B",
        .heads = { &s_dosing_heads[0], &s_dosing_heads[1] },
        .head_count = 2,
        .target_ec = 1.0f,
    },
};

// Function Prototypes
static void wait_for_touch(void);

static void prime_line_toggle(dosing_head_t *head);
static void target_dose_toggle(dose_group_t *group, float target_ec);
static void shot_dose_start(dosing_head_t *head);
static void prime_sample_timer_init(dosing_head_t *head);
static esp_err_t ba234_update_sensor_data(void);

// BA234 Sensor data stuct
esp_err_t ba234_sensor_status = ESP_ERR_INVALID_STATE;
ba234_sensor_data_t ba234_sensor_data;

#define BA234_BACKGROUND_SAMPLE_DELAY_MS 50U

// Set to 1 to log one combined line of all non-BA234 sensor readings each cycle
#define LOG_SENSORS 1

#define INTERVAL 400
#define WAIT wait_for_touch()

static const char *TAG = "ST7789";

// "Prime Line" routine: liquid sensor sampling parameters
#define PRIME_SAMPLE_PERIOD_US 1000U
#define PRIME_SAMPLE_WINDOW 100U
#define PRIME_STOP_THRESHOLD 70U

// Shot dose duration and target dose mix interval now live in the settings
// manager (NVS-backed, adjustable at runtime) — see settings_manager.h.

// EC history served to the web UI chart: sample period and window (3 minutes)
#define EC_HISTORY_SAMPLE_PERIOD_MS 2000U
#define EC_HISTORY_WINDOW_MS 180000U
#define EC_HISTORY_POINT_COUNT (EC_HISTORY_WINDOW_MS / EC_HISTORY_SAMPLE_PERIOD_MS)

// Grow light soft fade via LEDC PWM
#define GROW_LIGHT_PWM_FREQ_HZ    5000
#define GROW_LIGHT_PWM_RES        LEDC_TIMER_10_BIT   // 0-1023 duty steps
#define GROW_LIGHT_PWM_MODE       LEDC_LOW_SPEED_MODE
#define GROW_LIGHT_PWM_TIMER      LEDC_TIMER_0
#define GROW_LIGHT_PWM_CHANNEL    LEDC_CHANNEL_0
#define GROW_LIGHT_FADE_TIME_MS   1500

static bool s_pump_on = false;
static bool s_grow_light_on = false;
// Set by target_dose_task on any FreeRTOS task; consumed only by the ST7789
// display task, which is the only task allowed to touch LVGL.
static volatile bool s_dose_popup_pending = false;
static volatile uint32_t s_dose_popup_ec = 0;
static const char *s_dose_popup_tag = "";

// Pending hour/minute shown on the system-time steppers before the user
// taps "SET" to commit them via hydro_time_set_manual()
static uint8_t s_pending_hour = 0;
static uint8_t s_pending_minute = 0;
static bool s_fert_a_on = false;
static bool s_fert_b_on = false;
static bool g_touch_ready = false;
static volatile float g_ui_temperature_c = 24.0f;
static volatile float g_ui_humidity_pct = 58.0f;
static volatile uint8_t g_ui_water_level_pct = 72U;
static volatile uint16_t g_ui_distance_mm = 0U;
static volatile uint16_t g_ui_tds_ppm = 0U;
static volatile uint32_t g_ui_ec_us_cm = 0U;
static volatile uint32_t g_cal_raw_ec = 0U;   // raw EC (no k applied) for calibration page
static volatile float    g_cal_temp   = 0.0f; // temperature from BA234 at last read

#define CONFIG_WIDTH  240
#define CONFIG_HEIGHT 320
#define CONFIG_OFFSETX 0
#define CONFIG_OFFSETY 0

#define TOUCH_I2C_MASTER_FREQ_HZ 400000

// Ultrasonic calibration (distance in cm).
// FULL = sensor-to-water distance when tank is full.
// EMPTY = sensor-to-water distance when tank is empty.
#define ULTRASONIC_TANK_FULL_DISTANCE_CM 5U
#define ULTRASONIC_TANK_EMPTY_DISTANCE_CM 35U

// --- I2C Touch Driver (CST816S / FT6236) ---
#define I2C_MASTER_NUM              0               /*!< I2C master i2c port number */
#define I2C_MASTER_FREQ_HZ          400000          /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0               /*!< I2C master doesn't need buffer */

//#define CST816S_ADDR                0x15            /*!< Slave address of the CST816S touch controller */
#define FT6236_ADDR                 0x38            /*!< Alternative: Slave address of FT6236 */

// I2C Bus 0
static i2c_master_bus_handle_t i2c_bus_touch_handle;
static i2c_master_dev_handle_t ft6236_handle;
static SemaphoreHandle_t touch_interrupt_sem = NULL;
static volatile bool touch_data_ready = false;

// I2C Bus 1
static i2c_master_bus_handle_t i2c_bus_sensors_handle;
static i2c_master_dev_handle_t sht4x_handle;
static i2c_master_dev_handle_t dyp_handle;
static sht4x_t g_sht4x;
static bool g_sht4x_ready = false;
static bool g_dyp_ready = false;

static uint8_t calculate_tank_water_percent(uint16_t current_distance_cm)
{
    if (current_distance_cm <= ULTRASONIC_TANK_FULL_DISTANCE_CM) {
        return 100;
    }

    if (current_distance_cm >= ULTRASONIC_TANK_EMPTY_DISTANCE_CM) {
        return 0;
    }

    uint32_t span = (uint32_t)(ULTRASONIC_TANK_EMPTY_DISTANCE_CM - ULTRASONIC_TANK_FULL_DISTANCE_CM);
    uint32_t level_from_full = (uint32_t)(current_distance_cm - ULTRASONIC_TANK_FULL_DISTANCE_CM);
    return (uint8_t)(((span - level_from_full) * 100U) / span);
}

// I2C Bus for touch panel, I2C number 0
static esp_err_t i2c_master_init(void) {
    if (i2c_bus_touch_handle != NULL && ft6236_handle != NULL) {
        return ESP_OK;
    }

    i2c_master_bus_config_t touch_bus_config = {
        .i2c_port = I2C_MASTER_NUM,
        .sda_io_num = TP_SDA,
        .scl_io_num = TP_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&touch_bus_config, &i2c_bus_touch_handle);
    if (err != ESP_OK) {
        return err;
    }

    i2c_device_config_t ft6236_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = FT6236_ADDR,
        .scl_speed_hz = TOUCH_I2C_MASTER_FREQ_HZ,
    };

    err = i2c_master_bus_add_device(i2c_bus_touch_handle, &ft6236_config, &ft6236_handle);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}

// I2C Bus for the sensors, I2C number 1
static esp_err_t i2c_master_init_bus_sensors(void){
	i2c_master_bus_config_t i2c_mst_config = {
        .sda_io_num = SENSOR_I2C_SDA,
        .scl_io_num = SENSOR_I2C_SCL,
        .i2c_port = 1,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false       
    };

    return i2c_new_master_bus(&i2c_mst_config, &i2c_bus_sensors_handle);
}

static bool tp_read_point(uint16_t *x, uint16_t *y) {
    if (!g_touch_ready || ft6236_handle == NULL) {
        return false;
    }

    uint8_t data[6];
    uint8_t start_reg = 0x02;
    
    // Read registers starting from 0x02 (Status, touch ID, X, Y)
    // Register 0x02 is the number of touch points in most I2C controllers
    esp_err_t ret = i2c_master_transmit_receive(ft6236_handle, &start_reg, 1, data, sizeof(data), 10);

    if (ret != ESP_OK || data[0] == 0) { // If no touch points
        touch_data_ready = false;  // Clear flag when no touch detected
        return false;
    }

    // Extract X and Y (Format varies slightly, but usually High byte followed by Low byte)
    // Most standard CST816S and FT6236 put X in [1] and [2], Y in [3] and [4]
    *x = ((data[1] & 0x0F) << 8) | data[2];
    *y = ((data[3] & 0x0F) << 8) | data[4];
    touch_data_ready = false;  // Clear flag after reading
    
    return true;
}

// ISR handler for touch interrupt pin (TP_INT)
static void IRAM_ATTR tp_interrupt_handler(void *arg)
{
    (void)arg;
    // Set flag indicating touch data is available
    touch_data_ready = true;
    // Post semaphore from ISR to wake touch reading
    if (touch_interrupt_sem != NULL) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xSemaphoreGiveFromISR(touch_interrupt_sem, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken == pdTRUE) {
            portYIELD_FROM_ISR();
        }
    }
}

static void tp_init(void) {
    g_touch_ready = false;

    // Create semaphore for touch interrupt signaling
    if (touch_interrupt_sem == NULL) {
        touch_interrupt_sem = xSemaphoreCreateBinary();
        if (touch_interrupt_sem == NULL) {
            ESP_LOGE(TAG, "Failed to create touch interrupt semaphore");
        }
    }


    // These 4 pins are the hardware JTAG pins (MTDO, MTCK, MTDI, MTMS) on the ESP32-S3!
    // They are reserved at boot. We MUST reset them to use them as GPIO/I2C.
    gpio_reset_pin(TP_SCL);
    gpio_reset_pin(TP_SDA);
    gpio_reset_pin(TP_INT);
    gpio_reset_pin(TP_RST);
    
    // Hardware Reset Pin
    gpio_config_t out_conf = {
        .pin_bit_mask = (1ULL << TP_RST),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = 0,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&out_conf);
    
    // Interrupt Pin - configured for falling edge (active low)
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << TP_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_NEGEDGE  // Falling edge - touch controller pulls low on touch
    };
    gpio_config(&in_conf);

    // Reset Sequence
    gpio_set_level(TP_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TP_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(50));

    // Init I2C bus
    if (i2c_master_init() == ESP_OK) {
        ESP_LOGI(TAG, "I2C Touch Panel Initialized.");

        esp_err_t ret = i2c_master_probe(i2c_bus_touch_handle, FT6236_ADDR, 10);
        if (ret == ESP_OK) {
            g_touch_ready = true;
            ESP_LOGI(TAG, "Touch panel detected at address 0x%02x", FT6236_ADDR);
            
            // Register GPIO interrupt handler for touch interrupt pin
            gpio_isr_handler_add(TP_INT, tp_interrupt_handler, NULL);
            gpio_intr_enable(TP_INT);
            ESP_LOGI(TAG, "Touch interrupt enabled on GPIO %d", TP_INT);
        } else {
            ESP_LOGW(TAG, "Touch panel not detected at address 0x%02x (err=%s)", FT6236_ADDR, esp_err_to_name(ret));
        }
        
    } else {
        ESP_LOGE(TAG, "I2C Init Failed.");
    }
}

static inline uint32_t grow_light_duty_for_percent(uint8_t pct);

static void init_actuator_outputs(void)
{
    for (size_t i = 0; i < sizeof(s_dosing_heads) / sizeof(s_dosing_heads[0]); i++) {
        gpio_set_direction(s_dosing_heads[i].pump_gpio,  GPIO_MODE_OUTPUT);
        gpio_set_level(s_dosing_heads[i].pump_gpio,  OUTPUT_OFF_LEVEL);
        gpio_set_direction(s_dosing_heads[i].valve_gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(s_dosing_heads[i].valve_gpio, OUTPUT_OFF_LEVEL);
    }

#ifdef CIRCULATION_PUMP_GPIO
    gpio_set_direction(CIRCULATION_PUMP_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(CIRCULATION_PUMP_GPIO, OUTPUT_OFF_LEVEL);
#endif

    // Grow light is dimmed via LEDC PWM (not a plain relay switch) so it can
    // be soft-started/soft-stopped instead of snapping on/off.
    ledc_timer_config_t grow_light_timer = {
        .speed_mode       = GROW_LIGHT_PWM_MODE,
        .timer_num        = GROW_LIGHT_PWM_TIMER,
        .duty_resolution  = GROW_LIGHT_PWM_RES,
        .freq_hz          = GROW_LIGHT_PWM_FREQ_HZ,
        .clk_cfg          = LEDC_AUTO_CLK,
    };
    ledc_timer_config(&grow_light_timer);

    ledc_channel_config_t grow_light_channel = {
        .gpio_num   = GROW_LIGHT_GPIO,
        .speed_mode = GROW_LIGHT_PWM_MODE,
        .channel    = GROW_LIGHT_PWM_CHANNEL,
        .timer_sel  = GROW_LIGHT_PWM_TIMER,
        .duty       = grow_light_duty_for_percent(0),
        .hpoint     = 0,
    };
    ledc_channel_config(&grow_light_channel);
    ledc_fade_func_install(0);
}

// Converts a 0-100 brightness percentage into a raw LEDC duty value, taking
// OUTPUT_ON_LEVEL/OUTPUT_OFF_LEVEL polarity into account (V2's relay board is
// active-low, so "brighter" means the pin spends more time LOW, i.e. a lower
// duty value, and vice versa for V1's active-high wiring).
static inline uint32_t grow_light_duty_for_percent(uint8_t pct)
{
    if (pct > 100) pct = 100;
    uint32_t max_duty = (1U << GROW_LIGHT_PWM_RES) - 1U;
    uint32_t on_time = (max_duty * pct) / 100U;
    return (OUTPUT_ON_LEVEL != 0) ? on_time : (max_duty - on_time);
}

static void grow_light_fade_to(uint8_t pct)
{
    uint32_t target_duty = grow_light_duty_for_percent(pct);
    ledc_set_fade_with_time(GROW_LIGHT_PWM_MODE, GROW_LIGHT_PWM_CHANNEL,
                             target_duty, GROW_LIGHT_FADE_TIME_MS);
    ledc_fade_start(GROW_LIGHT_PWM_MODE, GROW_LIGHT_PWM_CHANNEL, LEDC_FADE_NO_WAIT);
}

static void ui_pump_switch_cb(bool is_on)
{
    s_pump_on = is_on;
#ifdef CIRCULATION_PUMP_GPIO
    gpio_set_level(CIRCULATION_PUMP_GPIO, is_on ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
#endif
    ESP_LOGI(TAG, "Circulation pump %s", is_on ? "ON" : "OFF");
    hydro_mqtt_publish_device_state(HYDRO_TOPIC_STATE_PUMP, is_on);
}

static void ui_light_switch_cb(bool is_on)
{
	s_grow_light_on = is_on;
	grow_light_fade_to(is_on ? 100 : 0);
	ESP_LOGI(TAG, "Grow light %s (fading)", is_on ? "ON" : "OFF");
    hydro_mqtt_publish_device_state(HYDRO_TOPIC_STATE_LIGHT, is_on);
}

static void ui_time_adjust_cb(kinetic_time_field_t field, bool increase)
{
    int delta = increase ? 1 : -1;
    switch (field) {
        case KINETIC_TIME_FIELD_SYSTEM_HOUR:
            s_pending_hour = (uint8_t)((s_pending_hour + 24 + delta) % 24);
            break;
        case KINETIC_TIME_FIELD_SYSTEM_MINUTE:
            s_pending_minute = (uint8_t)((s_pending_minute + 60 + delta) % 60);
            break;
        case KINETIC_TIME_FIELD_ON_HOUR: {
            uint8_t h, m;
            hydro_time_get_on_time(&h, &m);
            h = (uint8_t)((h + 24 + delta) % 24);
            hydro_time_set_on_time(h, m);
            break;
        }
        case KINETIC_TIME_FIELD_ON_MINUTE: {
            uint8_t h, m;
            hydro_time_get_on_time(&h, &m);
            m = (uint8_t)((m + 60 + delta) % 60);
            hydro_time_set_on_time(h, m);
            break;
        }
        case KINETIC_TIME_FIELD_OFF_HOUR: {
            uint8_t h, m;
            hydro_time_get_off_time(&h, &m);
            h = (uint8_t)((h + 24 + delta) % 24);
            hydro_time_set_off_time(h, m);
            break;
        }
        case KINETIC_TIME_FIELD_OFF_MINUTE: {
            uint8_t h, m;
            hydro_time_get_off_time(&h, &m);
            m = (uint8_t)((m + 60 + delta) % 60);
            hydro_time_set_off_time(h, m);
            break;
        }
    }
}

static void ui_time_set_cb(void)
{
    hydro_time_set_manual(s_pending_hour, s_pending_minute);
    ESP_LOGI(TAG, "System time manually set to %02u:%02u", s_pending_hour, s_pending_minute);
}

// NTP sync blocks on network I/O, so it runs on its own short-lived task and
// never on the display task (which must stay free to run LVGL).
static void ntp_sync_task(void *arg)
{
    (void)arg;
    hydro_time_sync_ntp();
    vTaskDelete(NULL);
}

static void ui_time_ntp_sync_cb(void)
{
    xTaskCreate(ntp_sync_task, "ntp_sync", 4096, NULL, 4, NULL);
}

static void ui_light_timer_enable_cb(bool enabled)
{
    hydro_time_set_timer_enabled(enabled);
    ESP_LOGI(TAG, "Grow light timer %s", enabled ? "ENABLED" : "DISABLED");
}

static void ui_fert_a_switch_cb(bool is_on)
{
    s_fert_a_on = is_on;
    gpio_set_level(FERT_A_VALVE_GPIO, is_on ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
    gpio_set_level(FERT_A_PUMP_GPIO,  is_on ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
    ESP_LOGI(TAG, "Fertilizer A %s", is_on ? "ON" : "OFF");
    hydro_mqtt_publish_device_state(HYDRO_TOPIC_STATE_FERT_A, is_on);
}

static void ui_fert_b_switch_cb(bool is_on)
{
    s_fert_b_on = is_on;
    gpio_set_level(FERT_B_VALVE_GPIO, is_on ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
    gpio_set_level(FERT_B_PUMP_GPIO,  is_on ? OUTPUT_ON_LEVEL : OUTPUT_OFF_LEVEL);
    ESP_LOGI(TAG, "Fertilizer B %s", is_on ? "ON" : "OFF");
    hydro_mqtt_publish_device_state(HYDRO_TOPIC_STATE_FERT_B, is_on);
}

// Routine buttons on the LVGL routines page
static void ui_routine_cb(kinetic_routine_t routine)
{
    switch (routine) {
    case KINETIC_ROUTINE_PRIME_A:
        prime_line_toggle(&s_dosing_heads[0]);
        break;
    case KINETIC_ROUTINE_PRIME_B:
        prime_line_toggle(&s_dosing_heads[1]);
        break;
    case KINETIC_ROUTINE_SHOT_A:
        shot_dose_start(&s_dosing_heads[0]);
        break;
    case KINETIC_ROUTINE_SHOT_B:
        shot_dose_start(&s_dosing_heads[1]);
        break;
    case KINETIC_ROUTINE_TARGET_AB:
        target_dose_toggle(&s_dose_groups[DOSE_GROUP_AB], s_dose_groups[DOSE_GROUP_AB].target_ec);
        break;
    default:
        break;
    }
}

// Settings sliders on the LVGL settings page
static void ui_shot_dose_setting_cb(uint32_t value_ms)
{
    settings_set_shot_dose_ms(value_ms);
}

static void ui_mix_interval_setting_cb(uint32_t value_ms)
{
    settings_set_mix_interval_ms(value_ms);
}

// +/- buttons beside the Target Dose A+B button
static void ui_ec_adjust_cb(bool increase)
{
    float ec = settings_get_target_ec() + (increase ? SETTINGS_TARGET_EC_STEP : -SETTINGS_TARGET_EC_STEP);
    settings_set_target_ec(ec); // setter clamps
    // The adjusted target applies to the A+B group the button starts
    s_dose_groups[DOSE_GROUP_AB].target_ec = settings_get_target_ec();
}

// --- EC Probe Single-Point Calibration ---
// Algorithm: ba234-ec-calibration-step3.md
// Settle 5 readings, then collect 25 at 2s intervals.
// k = 1413 / mean; stored to NVS via settings_manager.

#define CAL_SETTLE_COUNT    5
#define CAL_SAMPLE_COUNT    25
#define CAL_SAMPLE_PERIOD_MS 2000U
#define CAL_REF_US_CM       1413.0f
#define CAL_TEMP_LOW        24.0f
#define CAL_TEMP_HIGH       26.0f

typedef enum {
    EC_CAL_IDLE = 0,
    EC_CAL_SETTLING,
    EC_CAL_SAMPLING,
    EC_CAL_PASS,
    EC_CAL_FAIL,
} ec_cal_state_t;

static ec_cal_state_t  s_cal_state        = EC_CAL_IDLE;
static bool            s_cal_ignore_temp  = false;
static uint8_t         s_cal_ticks        = 0;
static float           s_cal_samples[CAL_SAMPLE_COUNT];
static uint8_t         s_cal_n            = 0;
static float           s_cal_mean         = 0.0f;
static float           s_cal_k            = 1.0f;
static const char     *s_cal_fail_reason  = "";

static void ec_cal_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (s_cal_state == EC_CAL_IDLE ||
        s_cal_state == EC_CAL_PASS ||
        s_cal_state == EC_CAL_FAIL) {
        return;
    }

    if (!s_cal_ignore_temp) {
        float temp = g_cal_temp;
        if (temp < CAL_TEMP_LOW || temp > CAL_TEMP_HIGH) {
            s_cal_fail_reason = "Temp out of 24-26C";
            return; // hold in current state without advancing
        }
        s_cal_fail_reason = ""; // temp OK now, clear warning
    }

    if (s_cal_state == EC_CAL_SETTLING) {
        s_cal_ticks++;
        if (s_cal_ticks >= CAL_SETTLE_COUNT) {
            s_cal_state = EC_CAL_SAMPLING;
            s_cal_n     = 0;
        }
        return;
    }

    if (s_cal_state == EC_CAL_SAMPLING) {
        s_cal_samples[s_cal_n++] = (float)g_cal_raw_ec;
        if (s_cal_n >= CAL_SAMPLE_COUNT) {
            float sum = 0.0f;
            for (int i = 0; i < CAL_SAMPLE_COUNT; i++) sum += s_cal_samples[i];
            float mean = sum / CAL_SAMPLE_COUNT;
            float var  = 0.0f;
            for (int i = 0; i < CAL_SAMPLE_COUNT; i++) {
                float d = s_cal_samples[i] - mean;
                var += d * d;
            }
            float stddev = sqrtf(var / CAL_SAMPLE_COUNT);

            if (mean < 1.0f) {
                s_cal_state = EC_CAL_FAIL;
                s_cal_fail_reason = "No signal";
                return;
            }
            if ((stddev / mean) >= 0.02f) {
                s_cal_state = EC_CAL_FAIL;
                s_cal_fail_reason = "Unstable >2%";
                return;
            }
            float pct_err = fabsf(mean - CAL_REF_US_CM) / CAL_REF_US_CM;
            if (pct_err > 0.30f) {
                s_cal_state = EC_CAL_FAIL;
                s_cal_fail_reason = "Mean >30% off 1413";
                return;
            }
            float k = CAL_REF_US_CM / mean;
            if (k < SETTINGS_EC_CAL_FACTOR_MIN || k > SETTINGS_EC_CAL_FACTOR_MAX) {
                s_cal_state = EC_CAL_FAIL;
                s_cal_fail_reason = "k out of 0.7-1.3";
                return;
            }
            s_cal_mean = mean;
            s_cal_k    = k;
            settings_set_ec_cal_factor(k);
            s_cal_state = EC_CAL_PASS;
        }
    }
}

static void ui_cal_start_cb(void)
{
    if (s_cal_state == EC_CAL_SETTLING || s_cal_state == EC_CAL_SAMPLING) {
        return;
    }
    s_cal_state       = EC_CAL_SETTLING;
    s_cal_ticks       = 0;
    s_cal_n           = 0;
    s_cal_fail_reason = "";
}

static void ui_cal_clear_cb(void)
{
    settings_clear_ec_cal_factor();
    s_cal_state = EC_CAL_IDLE;
}

static void ui_cal_ignore_temp_cb(void)
{
    s_cal_ignore_temp = !s_cal_ignore_temp;
}

static void wait_for_touch(void) {
    uint16_t tx, ty;
    
    // Check for a touch
    while (!tp_read_point(&tx, &ty)) {
        vTaskDelay(pdMS_TO_TICKS(10)); // tiny yield while idle
    }
    
    // Add a tiny debounce delay so we don't accidentally skip two slides from a single tap
    vTaskDelay(pdMS_TO_TICKS(100)); 
}

void traceHeap() {
	static uint32_t _free_heap_size = 0;
	if (_free_heap_size == 0) _free_heap_size = esp_get_free_heap_size();

	int _diff_free_heap_size = _free_heap_size - esp_get_free_heap_size();
	ESP_LOGI(__FUNCTION__, "_diff_free_heap_size=%d", _diff_free_heap_size);
	ESP_LOGI(__FUNCTION__, "esp_get_free_heap_size() : %6"PRIu32"\n", esp_get_free_heap_size() );
#if 0
	printf("esp_get_minimum_free_heap_size() : %6"PRIu32"\n", esp_get_minimum_free_heap_size() );
	printf("xPortGetFreeHeapSize() : %6zd\n", xPortGetFreeHeapSize() );
	printf("xPortGetMinimumEverFreeHeapSize() : %6zd\n", xPortGetMinimumEverFreeHeapSize() );
	printf("heap_caps_get_free_size(MALLOC_CAP_32BIT) : %6d\n", heap_caps_get_free_size(MALLOC_CAP_32BIT) );
	// that is the amount of stack that remained unused when the task stack was at its greatest (deepest) value.
	printf("uxTaskGetStackHighWaterMark() : %6d\n", uxTaskGetStackHighWaterMark(NULL));
#endif
}

static void my_disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p) {
    TFT_t *dev = (TFT_t *)disp_drv->user_data;
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);
    uint32_t size = w * h;

    uint16_t _x1 = area->x1 + dev->_offsetx;
    uint16_t _x2 = area->x2 + dev->_offsetx;
    uint16_t _y1 = area->y1 + dev->_offsety;
    uint16_t _y2 = area->y2 + dev->_offsety;

    // Send high-speed hardware window bounds
    spi_master_write_command(dev, 0x2A);
    spi_master_write_addr(dev, _x1, _x2);
    spi_master_write_command(dev, 0x2B);
    spi_master_write_addr(dev, _y1, _y2);
    spi_master_write_command(dev, 0x2C);

    // Blast the entire pre-swapped LVGL color buffer over SPI continuously
    gpio_set_level(dev->_dc, 1); // Switch to Data mode
    
    // Now that native SPI max_transfer_sz is unlocked, blast the full chunk instantly!
    spi_master_write_byte(dev->_SPIHandle, (const uint8_t *)color_p, size * 2);

    lv_disp_flush_ready(disp_drv);
}

static void my_touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data) {
    (void)indev_drv;

    if (!g_touch_ready) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    // Only read touch data if the interrupt flag is set (touch detected)
    if (!touch_data_ready) {
        data->state = LV_INDEV_STATE_REL;
        return;
    }

    uint16_t x_raw, y_raw;
    bool touched = tp_read_point(&x_raw, &y_raw);
    if(touched) {
        data->state = LV_INDEV_STATE_PR;
        
        // Proper Matrix: X correlates to the physical long-edge (y_raw)
        // Y correlates to the physical short-edge (x_raw)
        data->point.x = 320 - y_raw;
        data->point.y = x_raw;
        
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

static bool any_routine_active(void);

static void sensor_read_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
        // All non-BA234 sensor readings are collected into one log line per cycle
        char log_line[192];
        size_t pos = 0;

        if (g_sht4x_ready) {
            float temperature = 0.0f;
            float humidity = 0.0f;
            esp_err_t ret = sht4x_measure_high_precision(&g_sht4x, &temperature, &humidity);
            if (ret == ESP_OK) {
                float humidity_pct = humidity;
                if (humidity_pct < 0.0f) {
                    humidity_pct = 0.0f;
                } else if (humidity_pct > 100.0f) {
                    humidity_pct = 100.0f;
                }
                g_ui_temperature_c = temperature;
                g_ui_humidity_pct = humidity_pct;
                pos += snprintf(log_line + pos, sizeof(log_line) - pos,
                                "Temp: %.2f C, Humidity: %.2f %%", temperature, humidity);
            } else {
                pos += snprintf(log_line + pos, sizeof(log_line) - pos,
                                "SHT40 error: %s", esp_err_to_name(ret));
            }
        } else {
            pos += snprintf(log_line + pos, sizeof(log_line) - pos, "SHT40 offline");
        }

        if (g_dyp_ready) {
            uint16_t distance_mm = 0;
            esp_err_t dyp_ret = dyp_read_distance(&distance_mm, &dyp_handle);
            if (dyp_ret == ESP_OK) {
                uint8_t water_left_percent = calculate_tank_water_percent(distance_mm);
                g_ui_water_level_pct = water_left_percent;
                g_ui_distance_mm = distance_mm;
                pos += snprintf(log_line + pos, sizeof(log_line) - pos,
                                " | Distance: %u mm, Water left: %u%%", distance_mm, water_left_percent);
            } else {
                pos += snprintf(log_line + pos, sizeof(log_line) - pos,
                                " | DYP error: %s", esp_err_to_name(dyp_ret));
            }
        } else {
            pos += snprintf(log_line + pos, sizeof(log_line) - pos, " | DYP offline");
        }

        snprintf(log_line + pos, sizeof(log_line) - pos,
                 " | Liquid1: %d, Liquid2: %d",
                 gpio_get_level(LIQUID_SENSOR_A), gpio_get_level(LIQUID_SENSOR_B));

#if LOG_SENSORS
        ESP_LOGI("SENSORS", "%s", log_line);
#endif

        static uint32_t s_mqtt_ticks = 0;
        if (any_routine_active()) {
            s_mqtt_ticks = 0;
            hydro_mqtt_publish_sensors(
                g_ui_temperature_c, g_ui_humidity_pct,
                g_ui_water_level_pct, g_ui_distance_mm,
                g_ui_ec_us_cm, g_ui_tds_ppm,
                gpio_get_level(LIQUID_SENSOR_A) != 0,
                gpio_get_level(LIQUID_SENSOR_B) != 0);
        } else if (++s_mqtt_ticks >= (HYDRO_MQTT_PUBLISH_INTERVAL_S / 2U)) {
            s_mqtt_ticks = 0;
            hydro_mqtt_publish_sensors(
                g_ui_temperature_c, g_ui_humidity_pct,
                g_ui_water_level_pct, g_ui_distance_mm,
                g_ui_ec_us_cm, g_ui_tds_ppm,
                gpio_get_level(LIQUID_SENSOR_A) != 0,
                gpio_get_level(LIQUID_SENSOR_B) != 0);
        }

        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}

void ST7789(void *pvParameters)
{
	// Give the serial monitor time to connect over USB before printing important logs
	vTaskDelay(pdMS_TO_TICKS(3000));

    init_actuator_outputs();
	
	// Initialize Touch Controller
    tp_init();

	TFT_t dev;

	// Change SPI Clock Frequency
	spi_clock_speed(40000000); // 40MHz, exactly double the standard 20MHz limitation
	// spi_clock_speed(80000000); // 80MHz

	// Initialize SPI Hardware Pins
	spi_master_init(&dev, LCD_SDA, LCD_SCK, LCD_CS, LCD_WR, LCD_RESET, LCD_BL);
	
	// Initialize the display using horizontal dimensions
	lcdInit(&dev, 320, 240, CONFIG_OFFSETX, CONFIG_OFFSETY);

	// Force Hardware Landscape Rotation (MADCTL) with Standard RGB Mapping
	spi_master_write_command(&dev, 0x36);
	spi_master_write_data_byte(&dev, 0xA0);

	// Force Color Inversion OFF (Restore dark theme)
	lcdInversionOff(&dev);

    lv_init();
    static lv_disp_draw_buf_t draw_buf;
    static lv_color_t *buf1 = NULL;
    if (!buf1) {
        buf1 = heap_caps_malloc(320 * 40 * sizeof(lv_color_t), MALLOC_CAP_DMA);
        assert(buf1 != NULL);
    }
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, 320 * 40);
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &draw_buf;
    disp_drv.hor_res = 320;
    disp_drv.ver_res = 240;
    disp_drv.user_data = &dev;
    lv_disp_drv_register(&disp_drv);

    // Switch to the basic (no-shadow) theme. LVGL's default theme draws drop-shadows
    // on every lv_btn/lv_obj using the software renderer, which is slow enough to
    // starve the IDLE task and trigger the task watchdog during a tileview scroll.
    // lv_theme_basic applies no shadow styling; all our manual lv_obj_set_style_*
    // calls still take effect.
    lv_disp_set_theme(lv_disp_get_default(),
                      lv_theme_basic_init(lv_disp_get_default()));

    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = my_touchpad_read;
    lv_indev_drv_register(&indev_drv);

	kinetic_os_set_pump_switch_cb(ui_pump_switch_cb);
	kinetic_os_set_light_switch_cb(ui_light_switch_cb);
    kinetic_os_set_fertilizer_a_switch_cb(ui_fert_a_switch_cb);
    kinetic_os_set_fertilizer_b_switch_cb(ui_fert_b_switch_cb);
    kinetic_os_set_routine_cb(ui_routine_cb);
    kinetic_os_set_shot_dose_setting_cb(ui_shot_dose_setting_cb);
    kinetic_os_set_mix_interval_setting_cb(ui_mix_interval_setting_cb);
    kinetic_os_set_ec_adjust_cb(ui_ec_adjust_cb);
    kinetic_os_set_cal_start_cb(ui_cal_start_cb);
    kinetic_os_set_cal_clear_cb(ui_cal_clear_cb);
    kinetic_os_set_cal_ignore_temp_cb(ui_cal_ignore_temp_cb);
    kinetic_os_set_time_adjust_cb(ui_time_adjust_cb);
    kinetic_os_set_time_set_cb(ui_time_set_cb);
    kinetic_os_set_time_ntp_sync_cb(ui_time_ntp_sync_cb);
    kinetic_os_set_light_timer_enable_cb(ui_light_timer_enable_cb);

    kinetic_os_ui_init();

    lv_timer_create(ec_cal_timer_cb, CAL_SAMPLE_PERIOD_MS, NULL);

	kinetic_os_set_pump_state(s_pump_on);
	kinetic_os_set_light_state(s_grow_light_on);
    kinetic_os_set_fertilizer_a_state(s_fert_a_on);
    kinetic_os_set_fertilizer_b_state(s_fert_b_on);
    // Position the settings sliders at the persisted values
    kinetic_os_set_shot_dose_setting(settings_get_shot_dose_ms());
    kinetic_os_set_mix_interval_setting(settings_get_mix_interval_ms());

    // Seed the system-time steppers with whatever time we currently have
    // (survived-reboot value, or 00:00 on a cold boot)
    hydro_time_get_current(&s_pending_hour, &s_pending_minute, NULL);
    {
        uint8_t on_h, on_m, off_h, off_m;
        hydro_time_get_on_time(&on_h, &on_m);
        hydro_time_get_off_time(&off_h, &off_m);
        kinetic_os_set_light_on_time(on_h, on_m);
        kinetic_os_set_light_off_time(off_h, off_m);
        kinetic_os_set_light_timer_enabled(hydro_time_get_timer_enabled());
    }

    // Paint the first full frame before lighting the backlight, so the panel's
    // stale GRAM contents (or SPI init noise) are never visible to the user.
    // The draw buffer only covers a 40-row strip, so several handler passes
    // are needed to flush the whole 240-row screen at least once.
    for (int i = 0; i < 10; i++) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    lcdBacklightOn(&dev);

    while(1) {
        kinetic_os_set_wifi_name(wifi_is_connected() ? WIFI_SSID : NULL);
        if (g_sht4x_ready) {
            kinetic_os_set_temperature(g_ui_temperature_c);
            kinetic_os_set_humidity(g_ui_humidity_pct);
        } else {
            kinetic_os_set_temperature_not_detected();
            kinetic_os_set_humidity_not_detected();
        }
        if (g_dyp_ready) {
            kinetic_os_set_water_level(g_ui_water_level_pct);
            kinetic_os_set_distance(g_ui_distance_mm);
        } else {
            kinetic_os_set_water_level_not_detected();
            kinetic_os_set_distance_not_detected();
        }
        // Update chemistry metrics if BA234 sensor is available
        if (ba234_sensor_status == ESP_OK) {
            g_ui_tds_ppm  = ba234_sensor_data.tds;
            g_cal_raw_ec  = ba234_sensor_data.ec;
            g_cal_temp    = ba234_sensor_data.temperature;
            uint32_t cal_ec = (uint32_t)(ba234_sensor_data.ec * settings_get_ec_cal_factor() + 0.5f);
            g_ui_ec_us_cm = cal_ec;
            kinetic_os_set_tds(ba234_sensor_data.tds);
            kinetic_os_set_ec(cal_ec);
        } else {
            kinetic_os_set_tds_not_detected();
            kinetic_os_set_ec_not_detected();
        }
        // Mirror routine states onto the routines page buttons
        kinetic_os_set_routine_state(KINETIC_ROUTINE_PRIME_A, s_dosing_heads[0].prime_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_PRIME_B, s_dosing_heads[1].prime_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_SHOT_A, s_dosing_heads[0].shot_dose_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_SHOT_B, s_dosing_heads[1].shot_dose_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_TARGET_AB, s_dose_groups[DOSE_GROUP_AB].active);
        kinetic_os_set_target_ec_display(s_dose_groups[DOSE_GROUP_AB].target_ec);

        if (s_dose_popup_pending) {
            s_dose_popup_pending = false;
            kinetic_os_show_dose_complete_popup(s_dose_popup_tag, s_dose_popup_ec);
        }

        // Feed the clock & grow light timer page
        {
            uint8_t hh, mm, ss;
            hydro_time_get_current(&hh, &mm, &ss);
            kinetic_os_set_clock_display(hh, mm, ss, hydro_time_is_set());
            kinetic_os_set_time_edit_fields(s_pending_hour, s_pending_minute);

            const char *status_text;
            switch (hydro_time_get_sync_status()) {
                case HYDRO_TIME_SYNC_IN_PROGRESS:  status_text = "Syncing...";            break;
                case HYDRO_TIME_SYNC_FAIL_NO_WIFI: status_text = "NTP Failed: No WiFi";   break;
                case HYDRO_TIME_SYNC_FAIL_TIMEOUT: status_text = "NTP Failed: Timeout";   break;
                default:
                    switch (hydro_time_get_source()) {
                        case HYDRO_TIME_SOURCE_NTP:    status_text = "NTP Synced"; break;
                        case HYDRO_TIME_SOURCE_MANUAL: status_text = "Manual";     break;
                        default:                        status_text = "Not Set";   break;
                    }
                    break;
            }
            kinetic_os_set_time_sync_status(status_text);

            uint8_t on_h, on_m, off_h, off_m;
            hydro_time_get_on_time(&on_h, &on_m);
            hydro_time_get_off_time(&off_h, &off_m);
            kinetic_os_set_light_on_time(on_h, on_m);
            kinetic_os_set_light_off_time(off_h, off_m);
            kinetic_os_set_light_timer_enabled(hydro_time_get_timer_enabled());

            // Apply the grow light schedule here (display task) since
            // kinetic_os_set_light_state() must only be called from this task.
            if (hydro_time_get_timer_enabled() && hydro_time_is_set()) {
                bool desired = hydro_time_light_should_be_on();
                if (desired != s_grow_light_on) {
                    s_grow_light_on = desired;
                    grow_light_fade_to(desired ? 100 : 0);
                    kinetic_os_set_light_state(desired);
                    ESP_LOGI(TAG, "Grow light timer: auto %s", desired ? "ON" : "OFF");
                    hydro_mqtt_publish_device_state(HYDRO_TOPIC_STATE_LIGHT, desired);
                }
            }

            if (!hydro_time_get_timer_enabled()) {
                kinetic_os_set_light_schedule_status("Idle", false);
            } else if (!hydro_time_is_set()) {
                kinetic_os_set_light_schedule_status("Waiting for time", false);
            } else {
                bool on_now = hydro_time_light_should_be_on();
                kinetic_os_set_light_schedule_status(on_now ? "ON now" : "OFF now", on_now);
            }
        }
        // Feed calibration page
        {
            kinetic_cal_update_t cu;
            cu.raw_ec          = g_cal_raw_ec;
            cu.temperature     = g_cal_temp;
            cu.stored_k        = settings_get_ec_cal_factor();
            cu.ignore_temp_active = s_cal_ignore_temp;
            cu.running_avg     = 0;
            cu.n_samples       = 0;
            cu.seconds_remaining = 0;

            static char cal_status_buf[40];
            switch (s_cal_state) {
                case EC_CAL_IDLE:
                    if (s_cal_ignore_temp) {
                        cu.status_text = "Ready (temp ignored)";
                    } else {
                        cu.status_text = "Place probe in 1413 uS/cm";
                    }
                    break;
                case EC_CAL_SETTLING: {
                    if (s_cal_fail_reason[0] != '\0') {
                        snprintf(cal_status_buf, sizeof(cal_status_buf), "Wait: %s", s_cal_fail_reason);
                    } else {
                        snprintf(cal_status_buf, sizeof(cal_status_buf), "Settling %u/5", (unsigned)s_cal_ticks);
                    }
                    cu.status_text = cal_status_buf;
                    cu.seconds_remaining = (uint8_t)((CAL_SETTLE_COUNT - s_cal_ticks) * (CAL_SAMPLE_PERIOD_MS / 1000U));
                    break;
                }
                case EC_CAL_SAMPLING: {
                    snprintf(cal_status_buf, sizeof(cal_status_buf), "Sampling %u/25", (unsigned)s_cal_n);
                    cu.status_text = cal_status_buf;
                    cu.seconds_remaining = (uint8_t)((CAL_SAMPLE_COUNT - s_cal_n) * (CAL_SAMPLE_PERIOD_MS / 1000U));
                    cu.n_samples = s_cal_n;
                    if (s_cal_n > 0) {
                        float sum = 0.0f;
                        for (int i = 0; i < s_cal_n; i++) sum += s_cal_samples[i];
                        cu.running_avg = (uint32_t)(sum / s_cal_n + 0.5f);
                    }
                    break;
                }
                case EC_CAL_PASS: {
                    snprintf(cal_status_buf, sizeof(cal_status_buf), "PASS  k=%.3f", (double)s_cal_k);
                    cu.status_text = cal_status_buf;
                    cu.running_avg = (uint32_t)(s_cal_mean + 0.5f);
                    cu.n_samples   = CAL_SAMPLE_COUNT;
                    break;
                }
                case EC_CAL_FAIL: {
                    snprintf(cal_status_buf, sizeof(cal_status_buf), "FAIL: %s", s_cal_fail_reason);
                    cu.status_text = cal_status_buf;
                    break;
                }
            }
            kinetic_os_update_cal(&cu);
        }
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
}

// --- "Prime Line" routine ---
// Runs the dosing head until its liquid sensor reports fluid in the line.

static void prime_line_stop(dosing_head_t *head)
{
    gpio_set_level(head->pump_gpio,  OUTPUT_OFF_LEVEL);
    gpio_set_level(head->valve_gpio, OUTPUT_OFF_LEVEL);
    head->prime_active = false;

    if (head->prime_sample_timer != NULL && esp_timer_is_active(head->prime_sample_timer)) {
        esp_timer_stop(head->prime_sample_timer);
    }

    head->prime_sample_count = 0;
    head->prime_high_count = 0;
}

static void prime_sample_timer_callback(void *arg)
{
    dosing_head_t *head = (dosing_head_t *)arg;

    if (!head->prime_active) {
        return;
    }

    head->prime_sample_count++;
    if (gpio_get_level(head->liquid_sensor) == 1) {
        head->prime_high_count++;
    }

    if (head->prime_sample_count >= PRIME_SAMPLE_WINDOW) {
        if (head->prime_high_count > PRIME_STOP_THRESHOLD) {
            ESP_LOGI(head->tag,
                     "Liquid detected in %u/%u samples, stopping line prime",
                     (unsigned int)head->prime_high_count,
                     (unsigned int)head->prime_sample_count);
            prime_line_stop(head);
        } else {
            head->prime_sample_count = 0;
            head->prime_high_count = 0;
        }
    }
}

static void prime_sample_timer_init(dosing_head_t *head)
{
    if (head->prime_sample_timer != NULL) {
        return;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &prime_sample_timer_callback,
        .arg = head,
        .name = "prime_liquid_sample"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &head->prime_sample_timer));
}

static void prime_line_toggle(dosing_head_t *head)
{
    if (head->prime_active) {
        ESP_LOGI(head->tag, "Line prime stopped by user");
        prime_line_stop(head);
        return;
    }

    // If water is already present at the pipe
    if (gpio_get_level(head->liquid_sensor) == 1) {
        ESP_LOGI(head->tag, "Liquid is already in the line, prime will not start!");
        return;
    }

    prime_sample_timer_init(head);

    head->prime_sample_count = 0;
    head->prime_high_count = 0;

    if (esp_timer_is_active(head->prime_sample_timer)) {
        esp_timer_stop(head->prime_sample_timer);
    }
    esp_err_t err = esp_timer_start_periodic(head->prime_sample_timer, PRIME_SAMPLE_PERIOD_US);
    if (err != ESP_OK) {
        ESP_LOGE(head->tag, "Failed to start prime sample timer: %s", esp_err_to_name(err));
        return;
    }

    gpio_set_level(head->valve_gpio, OUTPUT_ON_LEVEL); // open valve before pump
    gpio_set_level(head->pump_gpio,  OUTPUT_ON_LEVEL);
    head->prime_active = true;

    ESP_LOGI(head->tag, "Line prime started!");
}

// --- "Shot Dose" routine ---
// Fires the dosing head for a fixed duration to inject one dose of fertilizer.

static void shot_dose_timer_callback(void *arg)
{
    dosing_head_t *head = (dosing_head_t *)arg;

    gpio_set_level(head->pump_gpio,  OUTPUT_OFF_LEVEL);
    gpio_set_level(head->valve_gpio, OUTPUT_OFF_LEVEL);
    head->shot_dose_active = false;
}

static void shot_dose_start(dosing_head_t *head)
{
    if (head->shot_dose_timer == NULL) {
        const esp_timer_create_args_t timer_args = {
            .callback = &shot_dose_timer_callback,
            .arg = head,
            .name = "shot_dose_off"
        };
        ESP_ERROR_CHECK(esp_timer_create(&timer_args, &head->shot_dose_timer));
    }

    if (esp_timer_is_active(head->shot_dose_timer)) {
        ESP_LOGI(head->tag, "Shot dose already in progress");
        return;
    }

    ESP_LOGI(head->tag, "Shot dose started");
    head->shot_dose_active = true;
    gpio_set_level(head->valve_gpio, OUTPUT_ON_LEVEL); // open valve before pump
    gpio_set_level(head->pump_gpio,  OUTPUT_ON_LEVEL);

    ESP_ERROR_CHECK(esp_timer_start_once(head->shot_dose_timer, (uint64_t)settings_get_shot_dose_ms() * 1000ULL));
}

// --- "Target Dose" routine ---
// Repeatedly shot-doses every head in the group until the EC sensor reaches the target setpoint.

static void target_dose_task(void *args)
{
    dose_group_t *group = (dose_group_t *)args;
    uint32_t target_ec = (uint32_t)(group->target_ec * 1000.0f); // mS/cm -> uS/cm
    bool target_reached = false;
    uint32_t reached_ec = 0;

    ESP_LOGI(group->tag, "Target dosing started, target EC: %u", (unsigned int)target_ec);

    if (ba234_sensor_status != ESP_OK) {
        for (size_t i = 0; i < group->head_count; i++) {
            gpio_set_level(group->heads[i]->pump_gpio,  OUTPUT_OFF_LEVEL);
            gpio_set_level(group->heads[i]->valve_gpio, OUTPUT_OFF_LEVEL);
        }
        ESP_LOGI(group->tag, "BA234 Sensor Disconnected!, will not initiate nutrient dosing!");
        group->active = false;
        vTaskDelete(NULL);
        return;
    }

    while (group->active) {
        ESP_LOGI(group->tag, "Reading EC sensor data...");
        ba234_update_sensor_data();

        uint32_t current_ec = (uint32_t)(ba234_sensor_data.ec * settings_get_ec_cal_factor() + 0.5f);
        ESP_LOGI(group->tag, "Current EC: %lu (raw %u), target EC: %u",
                 (unsigned long)current_ec, (unsigned int)ba234_sensor_data.ec, (unsigned int)target_ec);
        if (target_ec > current_ec) {
            // Dose the fertilizer if current concentration is less than target
            ESP_LOGI(group->tag, "Current EC is below target, dosing fertilizer...");
            for (size_t i = 0; i < group->head_count; i++) {
                shot_dose_start(group->heads[i]);
            }
        } else {
            ESP_LOGI(group->tag, "Target EC reached, stopping dosing loop");
            target_reached = true;
            reached_ec = current_ec;
            break;
        }
        // Let the nutrient mix before the next EC measurement
        vTaskDelay(pdMS_TO_TICKS(settings_get_mix_interval_ms()));
    }

    ESP_LOGI(group->tag, "Target dosing finished, target_reached=%s", target_reached ? "true" : "false");
    for (size_t i = 0; i < group->head_count; i++) {
        gpio_set_level(group->heads[i]->pump_gpio,  OUTPUT_OFF_LEVEL);
        gpio_set_level(group->heads[i]->valve_gpio, OUTPUT_OFF_LEVEL);
    }
    group->active = false;

    if (target_reached) {
        // Display task (ST7789) picks this up and shows the popup; LVGL
        // calls are only safe from that task.
        s_dose_popup_tag = group->tag;
        s_dose_popup_ec  = reached_ec;
        s_dose_popup_pending = true;
    }

    vTaskDelete(NULL);
}

// True if another active group is already dosing one of this group's heads
static bool dose_group_conflicts(const dose_group_t *group)
{
    for (size_t g = 0; g < DOSE_GROUP_COUNT; g++) {
        const dose_group_t *other = &s_dose_groups[g];
        if (other == group || !other->active) {
            continue;
        }
        for (size_t i = 0; i < group->head_count; i++) {
            for (size_t j = 0; j < other->head_count; j++) {
                if (group->heads[i] == other->heads[j]) {
                    return true;
                }
            }
        }
    }
    return false;
}

static void target_dose_toggle(dose_group_t *group, float target_ec)
{
    if (group->active) {
        // The dosing task notices the cleared flag and shuts the outputs off itself
        ESP_LOGI(group->tag, "Target dosing cancel requested");
        group->active = false;
        return;
    }

    if (dose_group_conflicts(group)) {
        ESP_LOGW(group->tag, "Another target dose is already using this head, not starting");
        return;
    }

    group->target_ec = target_ec;
    settings_set_target_ec(target_ec); // persist the last used target
    ESP_LOGI(group->tag, "Target EC setpoint set to %.1f", target_ec);

    group->active = true;
    if (xTaskCreate(target_dose_task, "target_dose", 4096, group, 10, NULL) != pdPASS) {
        ESP_LOGE(group->tag, "Failed to create target dose task");
        group->active = false;
    }
}

static float normalize_nutrient_concentration(float nutrient_concentration)
{
    if (nutrient_concentration < 0.0f) {
        nutrient_concentration = 0.0f;
    }

    int32_t tenths = (int32_t)(nutrient_concentration * 10.0f + 0.5f);
    return (float)tenths / 10.0f;
}

// Serializes BA234 reads between the background sampler and dosing tasks
static SemaphoreHandle_t s_ba234_mutex = NULL;

// LVGL is not thread-safe, so this function must not touch kinetic_os_*:
// it runs in the sensor and dosing tasks, while LVGL runs in the display task.
// The display task mirrors ba234_sensor_data onto the UI itself.
static esp_err_t ba234_update_sensor_data(void)
{
    if (s_ba234_mutex != NULL) {
        xSemaphoreTake(s_ba234_mutex, portMAX_DELAY);
    }
    esp_err_t err = ba234_read_data(&ba234_sensor_data);
    if (s_ba234_mutex != NULL) {
        xSemaphoreGive(s_ba234_mutex);
    }
    return err;
}

// EC history ring buffer feeding the web UI chart
static uint32_t s_ec_history[EC_HISTORY_POINT_COUNT];
static volatile uint16_t s_ec_history_count = 0;
static volatile uint16_t s_ec_history_head = 0; // next write index

static void ec_history_timer_callback(void *arg)
{
    (void)arg;

    if (ba234_sensor_status != ESP_OK) {
        return;
    }

    s_ec_history[s_ec_history_head] = ba234_sensor_data.ec;
    s_ec_history_head = (s_ec_history_head + 1) % EC_HISTORY_POINT_COUNT;
    if (s_ec_history_count < EC_HISTORY_POINT_COUNT) {
        s_ec_history_count++;
    }
}

static void ec_history_sampler_init(void)
{
    esp_timer_handle_t timer;

    const esp_timer_create_args_t timer_args = {
        .callback = &ec_history_timer_callback,
        .name = "ec_history_sample"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(timer, (uint64_t)EC_HISTORY_SAMPLE_PERIOD_MS * 1000ULL));
}

// --- Web module glue (see web_server.h) ---

// Builds the /api/status JSON
void hydro_build_status_json(char *buf, size_t buf_len)
{
    snprintf(buf, buf_len,
             "{\"light\":%s,"
             "\"prime_a\":%s,\"prime_b\":%s,"
             "\"target_dose_a\":%s,\"target_dose_b\":%s,\"target_dose_ab\":%s,"
             "\"shot_dose_a\":%s,\"shot_dose_b\":%s,"
             "\"target_ec_a\":%.1f,\"target_ec_b\":%.1f,\"target_ec_ab\":%.1f,"
             "\"temp\":%.1f,\"humidity\":%.1f,\"water\":%u,"
             "\"tds\":%u,\"ec\":%lu,\"distance\":%u,"
             "\"liquid1\":%s,\"liquid2\":%s}",
             s_grow_light_on ? "true" : "false",
             s_dosing_heads[0].prime_active ? "true" : "false",
             s_dosing_heads[1].prime_active ? "true" : "false",
             s_dose_groups[DOSE_GROUP_A].active ? "true" : "false",
             s_dose_groups[DOSE_GROUP_B].active ? "true" : "false",
             s_dose_groups[DOSE_GROUP_AB].active ? "true" : "false",
             s_dosing_heads[0].shot_dose_active ? "true" : "false",
             s_dosing_heads[1].shot_dose_active ? "true" : "false",
             s_dose_groups[DOSE_GROUP_A].target_ec,
             s_dose_groups[DOSE_GROUP_B].target_ec,
             s_dose_groups[DOSE_GROUP_AB].target_ec,
             g_ui_temperature_c,
             g_ui_humidity_pct,
             (unsigned)g_ui_water_level_pct,
             (unsigned)g_ui_tds_ppm,
             (unsigned long)g_ui_ec_us_cm,
             (unsigned)g_ui_distance_mm,
             gpio_get_level(LIQUID_SENSOR_A) ? "true" : "false",
             gpio_get_level(LIQUID_SENSOR_B) ? "true" : "false");
}

// Maps a device name suffix ("..._a" / "..._b") to its dosing head
static dosing_head_t *dosing_head_from_device(const char *device)
{
    size_t len = strlen(device);
    if (len < 2 || device[len - 2] != '_') {
        return NULL;
    }
    if (device[len - 1] == 'a') {
        return &s_dosing_heads[0];
    }
    if (device[len - 1] == 'b') {
        return &s_dosing_heads[1];
    }
    return NULL;
}

// Maps a target dose device suffix ("a" / "b" / "ab") to its dose group
static dose_group_t *dose_group_from_suffix(const char *suffix)
{
    if (strcmp(suffix, "a") == 0) {
        return &s_dose_groups[DOSE_GROUP_A];
    }
    if (strcmp(suffix, "b") == 0) {
        return &s_dose_groups[DOSE_GROUP_B];
    }
    if (strcmp(suffix, "ab") == 0) {
        return &s_dose_groups[DOSE_GROUP_AB];
    }
    return NULL;
}

// Executes a device command from /api/toggle. concentration < 0 means "not provided".
void hydro_web_toggle(const char *device, float concentration)
{
    if (strcmp(device, "light") == 0) {
        s_grow_light_on = !s_grow_light_on;
        grow_light_fade_to(s_grow_light_on ? 100 : 0);
        ESP_LOGI(TAG, "Grow light %s (web, fading)", s_grow_light_on ? "ON" : "OFF");
        hydro_mqtt_publish_device_state(HYDRO_TOPIC_STATE_LIGHT, s_grow_light_on);
        return;
    }

    if (strncmp(device, "target_dose_", 12) == 0) {
        dose_group_t *group = dose_group_from_suffix(device + 12);
        if (group == NULL) {
            return;
        }
        float target_ec = group->target_ec;
        if (concentration >= 0.0f) {
            target_ec = normalize_nutrient_concentration(concentration);
        }
        target_dose_toggle(group, target_ec);
        return;
    }

    dosing_head_t *head = dosing_head_from_device(device);
    if (head == NULL) {
        return;
    }
    if (strncmp(device, "prime_", 6) == 0) {
        prime_line_toggle(head);
    } else if (strncmp(device, "shot_dose_", 10) == 0) {
        shot_dose_start(head);
    }
}

// Builds the /api/ec_history JSON
const char *hydro_build_ec_history_json(void)
{
    // Handlers run serialized in the httpd task, so a static buffer is safe
    // and keeps ~1 KB off the httpd task stack.
    static char json_response[EC_HISTORY_POINT_COUNT * 11 + 48];

    uint16_t count = s_ec_history_count;
    uint16_t head = s_ec_history_head;
    size_t pos = 0;

    pos += snprintf(json_response + pos, sizeof(json_response) - pos,
                    "{\"period_ms\":%u,\"window_ms\":%u,\"ec\":[",
                    (unsigned)EC_HISTORY_SAMPLE_PERIOD_MS,
                    (unsigned)EC_HISTORY_WINDOW_MS);

    // Emit oldest -> newest
    for (uint16_t i = 0; i < count; i++) {
        uint16_t idx = (head + EC_HISTORY_POINT_COUNT - count + i) % EC_HISTORY_POINT_COUNT;
        pos += snprintf(json_response + pos, sizeof(json_response) - pos,
                        "%s%u", i ? "," : "", (unsigned)s_ec_history[idx]);
    }
    snprintf(json_response + pos, sizeof(json_response) - pos, "]}");

    return json_response;
}

void Sensors_Init(){
    esp_err_t ret = i2c_master_init_bus_sensors();
    if (ret != ESP_OK) {
        ESP_LOGE("SHT40", "I2C bus init failed: %s", esp_err_to_name(ret));
        g_sht4x_ready = false;
        return;
    }

    g_sht4x.i2c_dev = sht4x_handle;
    ret = sht4x_init(&g_sht4x, i2c_bus_sensors_handle, SHT40_I2C_ADDR_44);
    if (ret == ESP_OK){
        g_sht4x_ready = true;
        ESP_LOGI("SHT40", "Sensor Initialized");
    } else {
        g_sht4x_ready = false;
        ESP_LOGE("SHT40", "Sensor init failed: %s", esp_err_to_name(ret));
    }

    ret = init_ultrasonic_sensor(&i2c_bus_sensors_handle, &dyp_handle);
    if (ret == ESP_OK) {
        g_dyp_ready = true;
        ESP_LOGI("DYP_SENSOR", "Ultrasonic sensor initialized");
    } else {
        g_dyp_ready = false;
        ESP_LOGE("DYP_SENSOR", "Ultrasonic init failed: %s", esp_err_to_name(ret));
    }
}

// init GPIO for liquid sensor
void init_liquid_sensor_gpio(){
    gpio_set_direction(LIQUID_SENSOR_A, GPIO_MODE_INPUT);
    gpio_set_intr_type(LIQUID_SENSOR_A, GPIO_INTR_DISABLE);

    gpio_set_direction(LIQUID_SENSOR_B, GPIO_MODE_INPUT);
    gpio_set_intr_type(LIQUID_SENSOR_B, GPIO_INTR_DISABLE);
}

// True while any head is firing a shot dose; background EC sampling pauses then
static bool any_shot_dose_active(void)
{
    for (size_t i = 0; i < sizeof(s_dosing_heads) / sizeof(s_dosing_heads[0]); i++) {
        if (s_dosing_heads[i].shot_dose_active) {
            return true;
        }
    }
    return false;
}

static bool any_routine_active(void)
{
    for (size_t i = 0; i < sizeof(s_dosing_heads) / sizeof(s_dosing_heads[0]); i++) {
        if (s_dosing_heads[i].prime_active || s_dosing_heads[i].shot_dose_active) {
            return true;
        }
    }
    for (size_t g = 0; g < DOSE_GROUP_COUNT; g++) {
        if (s_dose_groups[g].active) {
            return true;
        }
    }
    return false;
}

void ba234_read_task(void * arg){
    
    // Initialize TDS/EC sensor
    ba234_sensor_status = ba234_init();

    if(ba234_sensor_status != ESP_OK){
        vTaskDelete(NULL);
        return;
    }

    while(1){
        if (!any_shot_dose_active()) {
            ba234_update_sensor_data();
        }
        // vTaskDelay(pdMS_TO_TICKS(BA234_BACKGROUND_SAMPLE_DELAY_MS)); 
        // There is no need for a long delay as sampling speed is limited by the sensor itself.
        // Average of 1880ms between samples.
    }
}

void app_main(void)
{
    // Restores the RTC-slow-memory time reference (if it survived a warm
    // reboot) before anything else touches the clock or the grow light timer
    hydro_time_init();

    // Load persisted dosing settings before anything uses them
    settings_manager_init();
    for (size_t i = 0; i < DOSE_GROUP_COUNT; i++) {
        s_dose_groups[i].target_ec = settings_get_target_ec();
    }

    s_ba234_mutex = xSemaphoreCreateMutex();

    // Initialize the "Prime Line" routine for both dosing heads
    gpio_install_isr_service(0);
    init_liquid_sensor_gpio();
    prime_sample_timer_init(&s_dosing_heads[0]);
    prime_sample_timer_init(&s_dosing_heads[1]);

    // Start sampling EC for the web UI chart
    ec_history_sampler_init();

    // Start WiFi connection
    wifi_init_sta();
    hydro_mqtt_init();

	Sensors_Init();
    xTaskCreate(sensor_read_task, "sensor_read_task", 4096, NULL, 4, NULL);
	
    xTaskCreate(ba234_read_task, "ba234_read_task", 1024*6, NULL, 2, NULL);

	xTaskCreate(ST7789, "ST7789", 1024*8, NULL, 2, NULL);
}
