#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "pinout.h"

static const char *TAG = "tp_sda_test";

void app_main(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << TP_SDA),
        .mode         = GPIO_MODE_OUTPUT_OD,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    uint32_t cycle = 0;
    while (1) {
        gpio_set_level(TP_SDA, 1);
        ESP_LOGI(TAG, "cycle %lu  SDA = HIGH", (unsigned long)cycle);
        vTaskDelay(pdMS_TO_TICKS(2000));

        gpio_set_level(TP_SDA, 0);
        ESP_LOGI(TAG, "cycle %lu  SDA = LOW", (unsigned long)cycle);
        vTaskDelay(pdMS_TO_TICKS(2000));

        cycle++;
    }
}
