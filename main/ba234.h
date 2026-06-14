#ifndef BA234_H
#define BA234_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief BA234 Sensor Data Structure
 * 
 * Contains all sensor measurements from the BA234 water quality sensor
 */
typedef struct {
    uint16_t tds;           /**< Total Dissolved Solids in ppm */
    uint32_t ec;            /**< Electrical Conductivity in µS/cm */
    uint16_t salinity_raw;  /**< Salinity raw value */
    float salinity;         /**< Salinity normalized value (raw / 100.0) */
    uint16_t sg_raw;        /**< Specific Gravity raw value */
    float specific_gravity; /**< Specific Gravity normalized value (raw / 10000.0) */
    uint16_t temp_raw;      /**< Temperature raw value */
    float temperature;      /**< Temperature in °C (raw / 10.0) */
    uint16_t hardness;      /**< Water hardness in ppm */
} ba234_sensor_data_t;

/**
 * @brief Initialize the BA234 UART peripheral
 * 
 * Configures UART1 with the sensor's communication parameters:
 * - Baud rate: 9600
 * - Data bits: 8
 * - Stop bits: 1
 * - No parity
 * - TX: GPIO 14, RX: GPIO 21
 * 
 * @return ESP_OK on success, ESP_ERR_* on failure
 */
esp_err_t ba234_init(void);

/**
 * @brief Read the latest sensor data from BA234
 * 
 * Sends a query command to the sensor and reads the response,
 * parsing all sensor values into the provided data structure.
 * Timeout: 100ms
 * 
 * @param data Pointer to ba234_sensor_data_t struct to store results
 * @return ESP_OK on successful read, ESP_TIMEOUT on read timeout, ESP_FAIL on parse error
 */
esp_err_t ba234_read_data(ba234_sensor_data_t *data);

#ifdef __cplusplus
}
#endif

#endif // BA234_H
