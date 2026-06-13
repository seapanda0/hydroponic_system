#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include <stdint.h>
#include <stddef.h>
#include <inttypes.h>

#define UART_BAUD 9600
#define UART_PORT_NUM 1

#define UART_PIN_TX GPIO_NUM_14
#define UART_PIN_RX GPIO_NUM_21

#define BUF_SIZE 1024
#define UART_TASK_STACK_SIZE 2048

const char* TAG = "BA234 SENSOR";

uint8_t rx_buf[128];

// (CRC code removed)

void app_main()
{
    vTaskDelay(pdMS_TO_TICKS(100));
    uart_config_t uart_config = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    int intr_alloc_flags = 0;

    ESP_ERROR_CHECK(uart_driver_install(UART_PORT_NUM, BUF_SIZE * 2, 0, 0, NULL, intr_alloc_flags));
    ESP_ERROR_CHECK(uart_param_config(UART_PORT_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT_NUM, UART_PIN_TX, UART_PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // Command to start sensor sampling
    uint8_t tx_buf[6] = {0xA0, 0x00, 0x00, 0x00, 0x00, 0xA0};

    while (1)
    {
        uart_write_bytes(UART_PORT_NUM, tx_buf, 6);
        uart_flush(UART_PORT_NUM);

        int rx_result;

        uint16_t tds, sal, sg, temperature, hardness;
        uint32_t ec;

        rx_result = uart_read_bytes(UART_PORT_NUM, rx_buf, 16, pdTICKS_TO_MS(100));
        if (rx_result){
            ESP_LOGI(TAG, "Received %d bytes: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                    rx_result, rx_buf[0], rx_buf[1], rx_buf[2], rx_buf[3], rx_buf[4], rx_buf[5], rx_buf[6], rx_buf[7],
                    rx_buf[8], rx_buf[9], rx_buf[10], rx_buf[11], rx_buf[12], rx_buf[13], rx_buf[14], rx_buf[15]);
            
            tds = (uint16_t)(((uint16_t)rx_buf[1] << 8) | (uint16_t)rx_buf[2]);
            ec  = ((uint32_t)rx_buf[3] << 24) | ((uint32_t)rx_buf[4] << 16) | ((uint32_t)rx_buf[5] << 8) | (uint32_t)rx_buf[6];
            sal = (uint16_t)(((uint16_t)rx_buf[7] << 8) | (uint16_t)rx_buf[8]);
            sg  = (uint16_t)(((uint16_t)rx_buf[9] << 8) | (uint16_t)rx_buf[10]);
            temperature  = (uint16_t)(((uint16_t)rx_buf[11] << 8) | (uint16_t)rx_buf[12]);
            hardness  = (uint16_t)(((uint16_t)rx_buf[13] << 8) | (uint16_t)rx_buf[14]);

            float sal_f = sal / 100.0f;       // sal was sent as value*100
            float sg_f  = sg  / 10000.0f;     // sg sent as value*10000
            float temp_f = temperature / 10.0f; // temperature sent as value*10

            ESP_LOGI(TAG, "TDS-H TDS-L: %02X %02X  -> TDS=%" PRIu16 " ppm", rx_buf[1], rx_buf[2], tds);
            ESP_LOGI(TAG, "EC-4 EC-3 EC-2 EC-1: %02X %02X %02X %02X  -> EC=%" PRIu32 " µS/cm",
                     rx_buf[3], rx_buf[4], rx_buf[5], rx_buf[6], ec);
            ESP_LOGI(TAG, "SAL-H SAL-L: %02X %02X  -> SAL=%" PRIu16 " (raw), %.2f",
                     rx_buf[7], rx_buf[8], sal, sal_f);
            ESP_LOGI(TAG, "SG-H SG-L: %02X %02X  -> SG=%" PRIu16 " (raw), %.4f",
                     rx_buf[9], rx_buf[10], sg, sg_f);
            ESP_LOGI(TAG, "TEM-H TEM-L: %02X %02X  -> TEMP=%" PRIu16 " (raw), %.1f°C",
                     rx_buf[11], rx_buf[12], temperature, temp_f);
            ESP_LOGI(TAG, "HAR-H HAR-L: %02X %02X  -> HARDNESS=%" PRIu16 " ppm",
                     rx_buf[13], rx_buf[14], hardness);
        }
        else
        {
            ESP_LOGI(TAG, "Failed to read from sensor!");
        }
        // vTaskDelay(pdMS_TO_TICKS(50));
    }
}