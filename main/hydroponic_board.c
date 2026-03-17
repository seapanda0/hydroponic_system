#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"

#include "lcd_patterns.h"

static const char *TAG = "ST7789";


void app_main(void)
{
	ESP_LOGI(TAG, "SPIFFS disabled (not mounted)");

	xTaskCreate(ST7789, "ST7789", 1024*6, NULL, 2, NULL);
}