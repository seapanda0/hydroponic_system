#ifndef ULTRASONIC_SENSOR_H_
#define ULTRASONIC_SENSOR_H_

/* I2C Sensor Driver for DYP-R01-V1.0 Sensor */
/* Datasheet obtained from manufacturer in Chinese */

#include "esp_check.h"

#define DYP_ADDR 0x74
#define I2C_MASTER_FREQ_HZ 400000

static const char* DYP_TAG = "DYP_SENSOR";

esp_err_t init_ultrasonic_sensor ();
esp_err_t dyp_read_distance(uint16_t *distance);

#endif