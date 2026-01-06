#include <stdio.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


void app_main(void)
{
    gpio_set_direction(GPIO_NUM_7, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_NUM_5, GPIO_MODE_OUTPUT);

    while(1){
        ESP_LOGI("Test", "Hello");
        gpio_set_level(GPIO_NUM_7, 0);
        gpio_set_level(GPIO_NUM_5, 0);
        vTaskDelay(300);
        gpio_set_level(GPIO_NUM_7, 1);
        gpio_set_level(GPIO_NUM_5, 1);
        vTaskDelay(300);
    }

}
