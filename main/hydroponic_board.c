#include "pinout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

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

#include "st7789.h"
#include "sht4x.h"
#include "ultrasonic_sensor.h"
#include "lvgl.h"
#include "kinetic_os.h"
#include "ba234.h"

// Web UI page embedded from webpage.html (see CMakeLists.txt EMBED_TXTFILES)
extern const char web_ui_html[] asm("_binary_webpage_html_start");

// WiFi and HTTP Server imports
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "wifi.h"

// Dosing head state. Each head is a pump + valve set wired in parallel on a single GPIO.
typedef struct {
    gpio_num_t gpio;                        // output driving the pump + valve set
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
        .gpio = FERT_A_GPIO,
        .liquid_sensor = LIQUID_SENSOR_1,
        .tag = "Dosing Head A",
    },
    {
        .gpio = FERT_B_GPIO,
        .liquid_sensor = LIQUID_SENSOR_2,
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
static void prime_line_toggle(dosing_head_t *head);
static void target_dose_toggle(dose_group_t *group, float target_ec);
static void shot_dose_start(dosing_head_t *head);
static void prime_sample_timer_init(dosing_head_t *head);
static esp_err_t ba234_update_sensor_data(void);

// BA234 Sensor data stuct
esp_err_t ba234_sensor_status = ESP_ERR_INVALID_STATE;
ba234_sensor_data_t ba234_sensor_data;

#define BA234_BACKGROUND_SAMPLE_DELAY_MS 50U

#define LOG_SHT40_SENSOR  1   // temperature and humidity readings
#define LOG_DYP_SENSOR    1   // ultrasonic distance and water level readings

#define INTERVAL 400
static void wait_for_touch(void);
#define WAIT wait_for_touch()

static const char *TAG = "ST7789";

// "Prime Line" routine: liquid sensor sampling parameters
#define PRIME_SAMPLE_PERIOD_US 1000U
#define PRIME_SAMPLE_WINDOW 100U
#define PRIME_STOP_THRESHOLD 70U

// "Shot Dose" routine: how long the output stays on for a single dose.
// Tune this to adjust the amount of fertilizer injected per dose.
#define SHOT_DOSE_DURATION_MS 500U

// "Target Dose" routine: time between a dose and the next EC measurement,
// so the nutrient has time to mix properly before being measured.
#define TARGET_DOSE_MIX_INTERVAL_MS 3000U

// Map grow light control to a physical output pin.
// Change this alias if your grow light relay is wired differently.
#define GROW_LIGHT_GPIO WS2811_CTRL

static bool s_pump_on = false;
static bool s_grow_light_on = false;
static bool s_fert_a_on = false;
static bool s_fert_b_on = false;
static bool g_touch_ready = false;
static volatile float g_ui_temperature_c = 24.0f;
static volatile float g_ui_humidity_pct = 58.0f;
static volatile uint8_t g_ui_water_level_pct = 72U;

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

static void init_actuator_outputs(void)
{
    for (size_t i = 0; i < sizeof(s_dosing_heads) / sizeof(s_dosing_heads[0]); i++) {
        gpio_set_direction(s_dosing_heads[i].gpio, GPIO_MODE_OUTPUT);
        gpio_set_level(s_dosing_heads[i].gpio, 0);
    }

	gpio_set_direction(GROW_LIGHT_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(GROW_LIGHT_GPIO, 0);
}

static void ui_pump_switch_cb(bool is_on)
{
	s_pump_on = is_on;
    ESP_LOGI(TAG, "Circulation pump %s", is_on ? "ON" : "OFF");
}

static void ui_light_switch_cb(bool is_on)
{
	s_grow_light_on = is_on;
	gpio_set_level(GROW_LIGHT_GPIO, is_on ? 1 : 0);
	ESP_LOGI(TAG, "Grow light %s", is_on ? "ON" : "OFF");
}

static void ui_fert_a_switch_cb(bool is_on)
{
    s_fert_a_on = is_on;
    gpio_set_level(FERT_A_GPIO, is_on ? 1 : 0);
    ESP_LOGI(TAG, "Fertilizer A pump %s", is_on ? "ON" : "OFF");
}

static void ui_fert_b_switch_cb(bool is_on)
{
    s_fert_b_on = is_on;
    gpio_set_level(FERT_B_GPIO, is_on ? 1 : 0);
    ESP_LOGI(TAG, "Fertilizer B pump %s", is_on ? "ON" : "OFF");
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

static void sensor_read_task(void *pvParameters)
{
    (void)pvParameters;

    while (1) {
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
#if LOG_SHT40_SENSOR
                ESP_LOGI("SHT40", "Temperature: %.2f C, Humidity: %.2f %%", temperature, humidity);
#endif
            } else {
#if LOG_SHT40_SENSOR
                ESP_LOGW("SHT40", "Read failed: %s", esp_err_to_name(ret));
#endif
            }
        }

        if (g_dyp_ready) {
            uint16_t distance_cm = 0;
            esp_err_t dyp_ret = dyp_read_distance(&distance_cm, &dyp_handle);
            if (dyp_ret == ESP_OK) {
                uint8_t water_left_percent = calculate_tank_water_percent(distance_cm);
                g_ui_water_level_pct = water_left_percent;
#if LOG_DYP_SENSOR
                ESP_LOGI("DYP_SENSOR", "Distance: %u cm, Water left: %u%%", distance_cm, water_left_percent);
#endif
            } else {
#if LOG_DYP_SENSOR
                ESP_LOGW("DYP_SENSOR", "Distance read failed: %s", esp_err_to_name(dyp_ret));
#endif
            }
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

    kinetic_os_ui_init();

	kinetic_os_set_pump_state(s_pump_on);
	kinetic_os_set_light_state(s_grow_light_on);
    kinetic_os_set_fertilizer_a_state(s_fert_a_on);
    kinetic_os_set_fertilizer_b_state(s_fert_b_on);

    while(1) {
        kinetic_os_set_temperature(g_ui_temperature_c);
        kinetic_os_set_humidity(g_ui_humidity_pct);
        kinetic_os_set_water_level(g_ui_water_level_pct);
        // Update chemistry metrics if BA234 sensor is available
        if (ba234_sensor_status == ESP_OK) {
            // TDS is provided in ppm
            kinetic_os_set_tds(ba234_sensor_data.tds);
            // EC is provided in us/cm by the sensor
            kinetic_os_set_ec(ba234_sensor_data.ec);
        }
        // Mirror routine states onto the routines page buttons
        kinetic_os_set_routine_state(KINETIC_ROUTINE_PRIME_A, s_dosing_heads[0].prime_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_PRIME_B, s_dosing_heads[1].prime_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_SHOT_A, s_dosing_heads[0].shot_dose_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_SHOT_B, s_dosing_heads[1].shot_dose_active);
        kinetic_os_set_routine_state(KINETIC_ROUTINE_TARGET_AB, s_dose_groups[DOSE_GROUP_AB].active);
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
}

// --- "Prime Line" routine ---
// Runs the dosing head until its liquid sensor reports fluid in the line.

static void prime_line_stop(dosing_head_t *head)
{
    gpio_set_level(head->gpio, 0);
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

    gpio_set_level(head->gpio, 1); // turn on the pump + valve set
    head->prime_active = true;

    ESP_LOGI(head->tag, "Line prime started!");
}

// --- "Shot Dose" routine ---
// Fires the dosing head for a fixed duration to inject one dose of fertilizer.

static void shot_dose_timer_callback(void *arg)
{
    dosing_head_t *head = (dosing_head_t *)arg;

    gpio_set_level(head->gpio, 0);
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
    gpio_set_level(head->gpio, 1);

    ESP_ERROR_CHECK(esp_timer_start_once(head->shot_dose_timer, (uint64_t)SHOT_DOSE_DURATION_MS * 1000ULL));
}

// --- "Target Dose" routine ---
// Repeatedly shot-doses every head in the group until the EC sensor reaches the target setpoint.

static void target_dose_task(void *args)
{
    dose_group_t *group = (dose_group_t *)args;
    uint32_t target_ec = (uint32_t)(group->target_ec * 1000.0f); // mS/cm -> uS/cm
    bool target_reached = false;

    ESP_LOGI(group->tag, "Target dosing started, target EC: %u", (unsigned int)target_ec);

    if (ba234_sensor_status != ESP_OK) {
        for (size_t i = 0; i < group->head_count; i++) {
            gpio_set_level(group->heads[i]->gpio, 0);
        }
        ESP_LOGI(group->tag, "BA234 Sensor Disconnected!, will not initiate nutrient dosing!");
        group->active = false;
        vTaskDelete(NULL);
        return;
    }

    while (group->active) {
        ESP_LOGI(group->tag, "Reading EC sensor data...");
        ba234_update_sensor_data();

        ESP_LOGI(group->tag, "Current EC: %u, target EC: %u", (unsigned int)ba234_sensor_data.ec, (unsigned int)target_ec);
        if (target_ec > ba234_sensor_data.ec) {
            // Dose the fertilizer if current concentration is less than target
            ESP_LOGI(group->tag, "Current EC is below target, dosing fertilizer...");
            for (size_t i = 0; i < group->head_count; i++) {
                shot_dose_start(group->heads[i]);
            }
        } else {
            ESP_LOGI(group->tag, "Target EC reached, stopping dosing loop");
            target_reached = true;
            break;
        }
        // Let the nutrient mix before the next EC measurement
        vTaskDelay(pdMS_TO_TICKS(TARGET_DOSE_MIX_INTERVAL_MS));
    }

    ESP_LOGI(group->tag, "Target dosing finished, target_reached=%s", target_reached ? "true" : "false");
    for (size_t i = 0; i < group->head_count; i++) {
        gpio_set_level(group->heads[i]->gpio, 0);
    }
    group->active = false;
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

static esp_err_t ba234_update_sensor_data(void)
{
    esp_err_t err = ba234_read_data(&ba234_sensor_data);
    if (err == ESP_OK) {
        kinetic_os_set_tds(ba234_sensor_data.tds);
        kinetic_os_set_ec(ba234_sensor_data.ec);
    }
    return err;
}

// HTTP Server endpoint handlers
static esp_err_t root_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, web_ui_html, HTTPD_RESP_USE_STRLEN);
}

static const httpd_uri_t root_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = root_get_handler,
    .user_ctx  = NULL
};

static void build_status_json(char *buf, size_t buf_len)
{
    snprintf(buf, buf_len,
             "{\"prime_a\":%s,\"prime_b\":%s,"
             "\"target_dose_a\":%s,\"target_dose_b\":%s,\"target_dose_ab\":%s,"
             "\"shot_dose_a\":%s,\"shot_dose_b\":%s,"
             "\"target_ec_a\":%.1f,\"target_ec_b\":%.1f,\"target_ec_ab\":%.1f,"
             "\"temp\":%.1f,\"humidity\":%.1f,\"water\":%d}",
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
             g_ui_water_level_pct);
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

static esp_err_t status_get_handler(httpd_req_t *req)
{
    char json_response[384];
    build_status_json(json_response, sizeof(json_response));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static const httpd_uri_t status_uri = {
    .uri       = "/api/status",
    .method    = HTTP_GET,
    .handler   = status_get_handler,
    .user_ctx  = NULL
};

static esp_err_t toggle_post_handler(httpd_req_t *req)
{
    char query[96];
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char device[16];
        if (httpd_query_key_value(query, "device", device, sizeof(device)) == ESP_OK) {
            if (strncmp(device, "target_dose_", 12) == 0) {
                dose_group_t *group = dose_group_from_suffix(device + 12);
                if (group != NULL) {
                    char concentration_str[16];
                    float target_ec = group->target_ec;
                    if (httpd_query_key_value(query, "concentration", concentration_str, sizeof(concentration_str)) == ESP_OK) {
                        char *endptr = NULL;
                        float requested_ec = strtof(concentration_str, &endptr);
                        if (endptr != concentration_str) {
                            target_ec = normalize_nutrient_concentration(requested_ec);
                        }
                    }
                    target_dose_toggle(group, target_ec);
                }
            } else {
                dosing_head_t *head = dosing_head_from_device(device);
                if (head != NULL) {
                    if (strncmp(device, "prime_", 6) == 0) {
                        prime_line_toggle(head);
                    } else if (strncmp(device, "shot_dose_", 10) == 0) {
                        shot_dose_start(head);
                    }
                }
            }
        }
    }

    char json_response[384];
    build_status_json(json_response, sizeof(json_response));

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, HTTPD_RESP_USE_STRLEN);
}

static const httpd_uri_t toggle_uri = {
    .uri       = "/api/toggle",
    .method    = HTTP_POST,
    .handler   = toggle_post_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    ESP_LOGI("WEB_SERVER", "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI("WEB_SERVER", "Registering URI handlers");
        httpd_register_uri_handler(server, &root_uri);
        httpd_register_uri_handler(server, &status_uri);
        httpd_register_uri_handler(server, &toggle_uri);
        return server;
    }

    ESP_LOGE("WEB_SERVER", "Error starting server!");
    return NULL;
}

// WiFi Event Handler and Setup
static int s_retry_num = 0;
#define WIFI_MAXIMUM_RETRY 5

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < WIFI_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI("WIFI", "retry to connect to the AP");
        } else {
            ESP_LOGI("WIFI", "failed to connect to the AP");
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI("WIFI", "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        
        // Update LVGL UI screen with connection status & SSID
        kinetic_os_set_wifi_name(WIFI_SSID);
        
        // Start web server when connected
        start_webserver();
    }
}

static void wifi_init_sta(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI("WIFI", "Initializing WiFi Station Mode...");
    ESP_ERROR_CHECK(esp_netif_init());

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_ERROR_CHECK(err);
    }
    
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("WIFI", "wifi_init_sta finished.");
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
    gpio_set_direction(LIQUID_SENSOR_1, GPIO_MODE_INPUT);
    gpio_set_intr_type(LIQUID_SENSOR_1, GPIO_INTR_DISABLE);

    gpio_set_direction(LIQUID_SENSOR_2, GPIO_MODE_INPUT);
    gpio_set_intr_type(LIQUID_SENSOR_2, GPIO_INTR_DISABLE);
}

// Set to 1 to enable liquid sensor debug logging task, 0 to disable
#define LIQUID_SENSOR_DEBUG 1

#if LIQUID_SENSOR_DEBUG
static void liquid_sensor_debug_task(void *pvParameters)
{
    (void)pvParameters;
    static const char *DBG_TAG = "LIQUID_DBG";
    while (1) {
        int s1 = gpio_get_level(LIQUID_SENSOR_1);
        int s2 = gpio_get_level(LIQUID_SENSOR_2);
        ESP_LOGI(DBG_TAG, "Sensor1 (GPIO%d)=%d  Sensor2 (GPIO%d)=%d",
                 LIQUID_SENSOR_1, s1, LIQUID_SENSOR_2, s2);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
#endif

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
        vTaskDelay(pdMS_TO_TICKS(BA234_BACKGROUND_SAMPLE_DELAY_MS)); 
        // There is no need for a long delay as sampling speed is limited by the sensor itself.
        // Average of 1880ms between samples.
    }
}

void app_main(void)
{
    // Initialize the "Prime Line" routine for both dosing heads
    gpio_install_isr_service(0);
    init_liquid_sensor_gpio();
    prime_sample_timer_init(&s_dosing_heads[0]);
    prime_sample_timer_init(&s_dosing_heads[1]);

#if LIQUID_SENSOR_DEBUG
    xTaskCreate(liquid_sensor_debug_task, "liquid_dbg", 2048, NULL, 1, NULL);
#endif

    // Start WiFi connection
    wifi_init_sta();

	Sensors_Init();
    xTaskCreate(sensor_read_task, "sensor_read_task", 4096, NULL, 4, NULL);
	
    xTaskCreate(ba234_read_task, "ba234_read_task", 1024*6, NULL, 2, NULL);

	xTaskCreate(ST7789, "ST7789", 1024*6, NULL, 2, NULL);
}
