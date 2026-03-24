#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/gptimer.h"

#include "pinout.h"
// #include "lcd_patterns.h"
#include "ultrasonic_sensor.h"

static const char *TAG = "hydroponic";

static gptimer_handle_t pump_timer;

static bool IRAM_ATTR pump_timer_on_alarm(
	gptimer_handle_t timer,
	const gptimer_alarm_event_data_t *edata,
	void *user_ctx)
{
	(void)timer;
	(void)edata;
	(void)user_ctx;

	gpio_set_level(PUMP_GPIO, 0);
	return false;
}

static void pump_timer_init(void)
{
	gptimer_config_t timer_config = {
		.clk_src = GPTIMER_CLK_SRC_DEFAULT,
		.direction = GPTIMER_COUNT_UP,
		.resolution_hz = 1000000,
	};
	ESP_ERROR_CHECK(gptimer_new_timer(&timer_config, &pump_timer));

	gptimer_event_callbacks_t callbacks = {
		.on_alarm = pump_timer_on_alarm,
	};
	ESP_ERROR_CHECK(gptimer_register_event_callbacks(pump_timer, &callbacks, NULL));

	gptimer_alarm_config_t alarm_config = {
		.alarm_count = 1000000,
		.reload_count = 0,
		.flags.auto_reload_on_alarm = false,
	};
	ESP_ERROR_CHECK(gptimer_set_alarm_action(pump_timer, &alarm_config));
	ESP_ERROR_CHECK(gptimer_enable(pump_timer));
}

static void pump_on_for_duration(uint32_t duration_ms)
{
	gpio_set_level(PUMP_GPIO, 1);

	gptimer_stop(pump_timer);
	ESP_ERROR_CHECK(gptimer_set_raw_count(pump_timer, 0));

	gptimer_alarm_config_t alarm_config = {
		.alarm_count = duration_ms * 1000,
		.reload_count = 0,
		.flags.auto_reload_on_alarm = false,
	};
	ESP_ERROR_CHECK(gptimer_set_alarm_action(pump_timer, &alarm_config));
	ESP_ERROR_CHECK(gptimer_start(pump_timer));
}

static void uart_cmd_task(void *arg)
{
	(void)arg;
	uint8_t rx_char = 0;

	while (1) {
		int len = uart_read_bytes(UART_NUM_0, &rx_char, 1, pdMS_TO_TICKS(100));
		if (len <= 0) {
			continue;
		}

		if (rx_char >= '1' && rx_char <= '9') {
			uint32_t duration_ms = (rx_char - '0') * 1000;
			ESP_LOGI(TAG, "Received '%c': pump ON for %lu ms", rx_char, duration_ms);
			pump_on_for_duration(duration_ms);
		} else if (rx_char == '0') {
			ESP_LOGI(TAG, "Received '0': stopping pump immediately");
			gpio_set_level(PUMP_GPIO, 0);
			gptimer_stop(pump_timer);
		}
	}
}

void app_main(void)
{

	gpio_set_direction(WS2811_CTRL, GPIO_MODE_OUTPUT);
	gpio_set_level(WS2811_CTRL, 1);

	gpio_set_direction(PUMP_GPIO, GPIO_MODE_OUTPUT);
	gpio_set_level(PUMP_GPIO, 0);

	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_0, 1024, 0, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_0, &uart_config));
	// 1'-'9' in the monitor terminal to run pump for 1-9 seconds (0 to stop immediately)
	pump_timer_init();

	xTaskCreate(uart_cmd_task, "uart_cmd_task", 2048, NULL, 5, NULL);

	ESP_LOGI(TAG, "Type 'a' in the monitor terminal to run pump for 1000 ms");2

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}

	
	// init_ultrasonic_sensor();

	// while(1){
	// 	uint16_t distance;
	// 	dyp_read_distance(&distance);
	// }

	// ESP_LOGI(TAG, "SPIFFS disabled (not mounted)");

	// xTaskCreate(ST7789, "ST7789", 1024*6, NULL, 2, NULL);


}