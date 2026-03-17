#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "lcd_patterns.h"
#include "ultrasonic_sensor.h"

static const char *TAG = "ST7789";


void app_main(void)
{
	init_ultrasonic_sensor();

	while(1){
		uint16_t distance;
		dyp_read_distance(&distance);
	}

	// ESP_LOGI(TAG, "SPIFFS disabled (not mounted)");

	// xTaskCreate(ST7789, "ST7789", 1024*6, NULL, 2, NULL);
}