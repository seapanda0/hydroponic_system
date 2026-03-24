#include "pinout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_vfs.h"
#include "driver/i2c_master.h"

#include "st7789.h"
#include "sht4x.h"
#include "ultrasonic_sensor.h"
#include "fontx.h"
#include "lvgl.h"
#include "kinetic_os.h"

#define INTERVAL 400
static void wait_for_touch(void);
#define WAIT wait_for_touch()

static const char *TAG = "ST7789";

// Map grow light control to a physical output pin.
// Change this alias if your grow light relay is wired differently.
#define GROW_LIGHT_GPIO WS2811_CTRL

static bool s_pump_on = false;
static bool s_grow_light_on = false;
static bool g_touch_ready = false;

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
static esp_err_t i2c_master_init(void)
{
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
        return false;
    }

    // Extract X and Y (Format varies slightly, but usually High byte followed by Low byte)
    // Most standard CST816S and FT6236 put X in [1] and [2], Y in [3] and [4]
    *x = ((data[1] & 0x0F) << 8) | data[2];
    *y = ((data[3] & 0x0F) << 8) | data[4];
    
    return true;
}

static void tp_init(void) {
    g_touch_ready = false;

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
    
    // Interrupt Pin
    gpio_config_t in_conf = {
        .pin_bit_mask = (1ULL << TP_INT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = 1,
        .pull_down_en = 0,
        .intr_type = GPIO_INTR_DISABLE
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
        } else {
            ESP_LOGW(TAG, "Touch panel not detected at address 0x%02x (err=%s)", FT6236_ADDR, esp_err_to_name(ret));
        }
        
    } else {
        ESP_LOGE(TAG, "I2C Init Failed.");
    }
}

static void init_actuator_outputs(void)
{
	gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_direction(GROW_LIGHT_GPIO, GPIO_MODE_OUTPUT);

	gpio_set_level(PUMP_GPIO, 0);
	gpio_set_level(GROW_LIGHT_GPIO, 0);
}

static void ui_pump_switch_cb(bool is_on)
{
	s_pump_on = is_on;
	gpio_set_level(PUMP_GPIO, is_on ? 1 : 0);
	ESP_LOGI(TAG, "Pump %s", is_on ? "ON" : "OFF");
}

static void ui_light_switch_cb(bool is_on)
{
	s_grow_light_on = is_on;
	gpio_set_level(GROW_LIGHT_GPIO, is_on ? 1 : 0);
	ESP_LOGI(TAG, "Grow light %s", is_on ? "ON" : "OFF");
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
                ESP_LOGI("SHT40", "Temperature: %.2f C, Humidity: %.2f %%", temperature, humidity);
            } else {
                ESP_LOGW("SHT40", "Read failed: %s", esp_err_to_name(ret));
            }
        }

        if (g_dyp_ready) {
            uint16_t distance_cm = 0;
            esp_err_t dyp_ret = dyp_read_distance(&distance_cm, &dyp_handle);
            if (dyp_ret == ESP_OK) {
                uint8_t water_left_percent = calculate_tank_water_percent(distance_cm);
                ESP_LOGI("DYP_SENSOR", "Distance: %u cm, Water left: %u%%", distance_cm, water_left_percent);
            } else {
                ESP_LOGW("DYP_SENSOR", "Distance read failed: %s", esp_err_to_name(dyp_ret));
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

	// set font file
	FontxFile fx16G[2];
	FontxFile fx24G[2];
	FontxFile fx32G[2];
	FontxFile fx32L[2];
	InitFontx(fx16G,"/fonts/ILGH16XB.FNT",""); // 8x16Dot Gothic
	InitFontx(fx24G,"/fonts/ILGH24XB.FNT",""); // 12x24Dot Gothic
	InitFontx(fx32G,"/fonts/ILGH32XB.FNT",""); // 16x32Dot Gothic
	InitFontx(fx32L,"/fonts/LATIN32B.FNT",""); // 16x32Dot Latin

	FontxFile fx16M[2];
	FontxFile fx24M[2];
	FontxFile fx32M[2];
	InitFontx(fx16M,"/fonts/ILMH16XB.FNT",""); // 8x16Dot Mincyo
	InitFontx(fx24M,"/fonts/ILMH24XB.FNT",""); // 12x24Dot Mincyo
	InitFontx(fx32M,"/fonts/ILMH32XB.FNT",""); // 16x32Dot Mincyo
	
	TFT_t dev;

	// Change SPI Clock Frequency
	spi_clock_speed(40000000); // 40MHz, exactly double the standard 20MHz limitation
	//spi_clock_speed(60000000); // 60MHz

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

    kinetic_os_ui_init();

	kinetic_os_set_pump_state(s_pump_on);
	kinetic_os_set_light_state(s_grow_light_on);

    while(1) {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
        lv_tick_inc(10);
    }
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

void app_main(void)
{
	Sensors_Init();
    xTaskCreate(sensor_read_task, "sensor_read_task", 4096, NULL, 4, NULL);
	
	xTaskCreate(ST7789, "ST7789", 1024*6, NULL, 2, NULL);
}
