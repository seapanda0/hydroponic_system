#include "ultrasonic_sensor.h"
#include "driver/i2c_master.h"

#include "pinout.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_check.h"
#include "esp_log.h"

i2c_master_bus_handle_t i2c_mst_handle;
i2c_master_dev_handle_t dyp_handle;

esp_err_t dyp_read_distance(uint16_t *distance){

    uint8_t write_buf[2];
    uint8_t read_buf[2];
    write_buf[0] = 0x10; // I2C Address
    write_buf[1] = 0xB0; // Command to start ranging

    esp_err_t err = i2c_master_transmit(dyp_handle, write_buf, 2, 100);
    if (err == ESP_OK){
        // ESP_LOGI(DYP_TAG, "Started ranging successfully");
    }else{
        ESP_LOGI(DYP_TAG, "Failed to start ranging");
        return err;
    }

    vTaskDelay(pdMS_TO_TICKS(40)); // Wait for at least 40ms for ranging to finish

    write_buf[0] = 0x02;
    err = i2c_master_transmit_receive(dyp_handle, write_buf, 1, read_buf, 2, 100);
    if (err == ESP_OK){
        *distance = ((uint16_t)read_buf[0] << 8) | (uint16_t)read_buf[1];
        ESP_LOGI(DYP_TAG, "Current distance: %d cm", *distance);
    }else{
        ESP_LOGI(DYP_TAG, "Failed to read distance from the sensor");
        return err;
    }
    
    return err;
}

esp_err_t init_ultrasonic_sensor () {

    i2c_master_bus_config_t i2c_mst_config = {
        .sda_io_num = SENSOR_I2C_SDA,
        .scl_io_num = SENSOR_I2C_SCL,
        .i2c_port = 0,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false       
    };
    i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = DYP_ADDR,
        .scl_speed_hz = I2C_MASTER_FREQ_HZ
    }; 
    i2c_new_master_bus(&i2c_mst_config, &i2c_mst_handle);
     
    i2c_master_bus_add_device(i2c_mst_handle, &device_config, &dyp_handle);

    esp_err_t err = i2c_master_probe(i2c_mst_handle, DYP_ADDR, 100);
    if (err == ESP_OK){
        ESP_LOGI(DYP_TAG, "Sensor Detected");
    }else {
        ESP_LOGI(DYP_TAG, "Sensor NOT Detected");
        return err;        
    }

    uint8_t write_buf[2] = {0x00, 0x00};
    uint8_t read_buf[2] = {0};
    err = i2c_master_transmit_receive(dyp_handle, write_buf, 1, read_buf, 2, 100);
    if (err == ESP_OK){
        ESP_LOGI(DYP_TAG, "Detected firmware version: 0x%02x%02x", read_buf[0], read_buf[1]);
    }else{
        ESP_LOGI(DYP_TAG, "Failed to read firmware version");
        return err;
    }
    uint16_t distance;
    err = dyp_read_distance(&distance);
    return err;
}