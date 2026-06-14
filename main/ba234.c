#include "ba234.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>
#include <string.h>
#include "pinout.h"

#define UART_BAUD 9600
#define UART_PORT_NUM 1

#define BUF_SIZE 1024
#define RX_BUF_SIZE 16
#define UART_READ_TIMEOUT_MS 10000

static const char* TAG = "BA234 SENSOR";
static bool ba234_initialized = false;

/**
 * @brief Initialize the BA234 UART peripheral
 */
esp_err_t ba234_init(void)
{
    if (ba234_initialized) {
        ESP_LOGW(TAG, "BA234 already initialized");
        return ESP_OK;
    }

    vTaskDelay(pdMS_TO_TICKS(20));

    uart_config_t uart_config = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

    esp_err_t err = uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_param_config(UART_PORT_NUM, &uart_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART: %s", esp_err_to_name(err));
        return err;
    }

    err = uart_set_pin(UART_PORT_NUM, SENSOR_UART_TX, SENSOR_UART_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set UART pins: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t tx_buf[6] = {0xA0, 0x00, 0x00, 0x00, 0x00, 0xA0};
    uart_write_bytes(UART_PORT_NUM, tx_buf, 6);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint8_t rx_buf[RX_BUF_SIZE] = {0};
    int rx_result = uart_read_bytes(UART_PORT_NUM, rx_buf, 6, pdMS_TO_TICKS(UART_READ_TIMEOUT_MS));
    
    // If bytes received and in the sequence of AC 02 00 00 00 AE (sensor busy) or 0xAA (normal data) as per datasheet
    if (!(rx_result <= 0) && (rx_buf[0] ==  0xAC || rx_buf[0] ==  0xAA )){
        ba234_initialized = true;
        ESP_LOGI(TAG, "BA234 UART initialized successfully");
        uart_flush(UART_PORT_NUM);
        return ESP_OK;
    }else{
        ba234_initialized = false;
        ESP_LOGW(TAG, "BA234 intialization failed!");
        uart_flush(UART_PORT_NUM);
        return ESP_FAIL;
    }
}

/**
 * @brief Read the latest sensor data from BA234
 */
esp_err_t ba234_read_data(ba234_sensor_data_t *data)
{
    if (!ba234_initialized){
        ESP_LOGE(TAG, "BA234 not initialized. Call ba234_init() first");
        return ESP_ERR_INVALID_STATE;
    }

    if (data == NULL)
    {
        ESP_LOGE(TAG, "Data pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    // Command to query sensor for sampling data
    uint8_t tx_buf[6] = {0xA0, 0x00, 0x00, 0x00, 0x00, 0xA0};
    uint8_t rx_buf[RX_BUF_SIZE] = {0};

    // Send query command
    uart_write_bytes(UART_PORT_NUM, tx_buf, 6);
    uart_flush(UART_PORT_NUM);

    // Read response with timeout
    int rx_result = uart_read_bytes(UART_PORT_NUM, rx_buf, 16, pdMS_TO_TICKS(UART_READ_TIMEOUT_MS));

    if (rx_result <= 0)
    {
        ESP_LOGW(TAG, "Failed to read from sensor (received %d bytes)", rx_result);
        return ESP_ERR_TIMEOUT;
    }
    else
    {
        // Parse sensor data from received bytes
        data->tds = (uint16_t)(((uint16_t)rx_buf[1] << 8) | (uint16_t)rx_buf[2]);
        data->ec = ((uint32_t)rx_buf[3] << 24) | ((uint32_t)rx_buf[4] << 16) | ((uint32_t)rx_buf[5] << 8) | (uint32_t)rx_buf[6];
        data->salinity_raw = (uint16_t)(((uint16_t)rx_buf[7] << 8) | (uint16_t)rx_buf[8]);
        data->sg_raw = (uint16_t)(((uint16_t)rx_buf[9] << 8) | (uint16_t)rx_buf[10]);
        data->temp_raw = (uint16_t)(((uint16_t)rx_buf[11] << 8) | (uint16_t)rx_buf[12]);
        data->hardness = (uint16_t)(((uint16_t)rx_buf[13] << 8) | (uint16_t)rx_buf[14]);

        // Convert raw values to normalized floating-point values
        data->salinity = data->salinity_raw / 100.0f;
        data->specific_gravity = data->sg_raw / 10000.0f;
        data->temperature = data->temp_raw / 10.0f;

        // ESP_LOGI(TAG, "Successfully (received %d bytes)", rx_result);
        // ESP_LOGI(TAG, "Received %d bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
        //          rx_result, rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3], rx_buf[4], rx_buf[5], rx_buf[6], rx_buf[7],
        //          rx_buf[8], rx_buf[9], rx_buf[10], rx_buf[11], rx_buf[12], rx_buf[13], rx_buf[14], rx_buf[15]);

        // Log parsed data
        // ESP_LOGI(TAG, "TDS=%" PRIu16 " ppm", data->tds);
        // ESP_LOGI(TAG, "EC=%" PRIu32 " µS/cm", data->ec);
        // ESP_LOGI(TAG, "Salinity=%.2f (raw %" PRIu16 ")", data->salinity, data->salinity_raw);
        // ESP_LOGI(TAG, "Specific Gravity=%.4f (raw %" PRIu16 ")", data->specific_gravity, data->sg_raw);
        // ESP_LOGI(TAG, "Temperature=%.1f°C (raw %" PRIu16 ")", data->temperature, data->temp_raw);
        // ESP_LOGI(TAG, "Hardness=%" PRIu16 " ppm", data->hardness);

        ESP_LOGI(TAG, "Successfully (received %d bytes), rx_result TDS=%" PRIu16 " ppm | EC=%" PRIu32 " µS/cm", rx_result ,data->tds, data->ec);

    }
    return ESP_OK;
}