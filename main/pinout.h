#ifndef PINOUT_H
#define PINOUT_H

#include "driver/gpio.h"

#define DBG_LED  GPIO_NUM_17

#define LCD_WR     GPIO_NUM_13 // Data control pin, labelled as RS in schematic
#define LCD_BL     GPIO_NUM_4
#define LCD_CS     GPIO_NUM_10
#define LCD_SDA    GPIO_NUM_12
#define LCD_SCK    GPIO_NUM_11
#define LCD_RESET  GPIO_NUM_9

#define TP_SCL GPIO_NUM_42
#define TP_SDA GPIO_NUM_41
#define TP_INT GPIO_NUM_40
#define TP_RST GPIO_NUM_39

#define SENSOR_I2C_SCL GPIO_NUM_2
#define SENSOR_I2C_SDA GPIO_NUM_1


#define SENSOR_UART_TX GPIO_NUM_14
#define SENSOR_UART_RX GPIO_NUM_21

#define RTC_RST GPIO_NUM_18
#define RTC_CLK GPIO_NUM_47
#define RTC_SDA GPIO_NUM_45

#define SD_CLK GPIO_NUM_3
#define SD_CMD GPIO_NUM_38
#define SD_D0  GPIO_NUM_8

#define FAN_GPIO         GPIO_NUM_7
#define HUMIDIFIER_GPIO  GPIO_NUM_6
#define PUMP_GPIO        GPIO_NUM_5

#define WS2811_CTRL GPIO_NUM_15
#define WS2811_D GPIO_NUM_16

#define LIQUID_SENSOR_1 GPIO_NUM_45
#define LIQUID_SENSOR_2 GPIO_NUM_46

#endif