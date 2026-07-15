#ifndef PINOUT_H
#define PINOUT_H

#include "driver/gpio.h"

#define BOARD_V1 0
#define BOARD_V2 1

#if BOARD_V2

#define DBG_LED  GPIO_NUM_46

#define LCD_WR     GPIO_NUM_40 // Data control pin, labelled as RS in schematic
#define LCD_BL     GPIO_NUM_47
#define LCD_CS     GPIO_NUM_38
#define LCD_SDA    GPIO_NUM_45
#define LCD_SCK    GPIO_NUM_48
#define LCD_RESET  GPIO_NUM_39

#define TP_SCL GPIO_NUM_1
#define TP_SDA GPIO_NUM_2
#define TP_INT GPIO_NUM_42
#define TP_RST GPIO_NUM_41

#define SENSOR_I2C_SCL GPIO_NUM_12
#define SENSOR_I2C_SDA GPIO_NUM_13

#define SENSOR_UART_TX GPIO_NUM_14
#define SENSOR_UART_RX GPIO_NUM_21

#define SD_CLK GPIO_NUM_10
#define SD_CMD GPIO_NUM_9
#define SD_D0  GPIO_NUM_11

#define SWITCH_1_GPIO GPIO_NUM_16
#define SWITCH_2_GPIO GPIO_NUM_15
#define SWITCH_3_GPIO GPIO_NUM_7
#define SWITCH_4_GPIO GPIO_NUM_6
#define SWITCH_5_GPIO GPIO_NUM_5
#define SWITCH_6_GPIO GPIO_NUM_4

#define IO_5V_1 GPIO_NUM_17
#define IO_5V_2 GPIO_NUM_18

// Each fertilizer output drives a full dosing set (pump + valve wired in parallel)
#define FERT_A_PUMP_GPIO       SWITCH_2_GPIO
#define FERT_A_VALVE_GPIO      SWITCH_1_GPIO

#define FERT_B_PUMP_GPIO       SWITCH_4_GPIO
#define FERT_B_VALVE_GPIO      SWITCH_3_GPIO

#define CIRCULATION_PUMP_GPIO  SWITCH_5_GPIO
#define GROW_LIGHT_GPIO SWITCH_6_GPIO

#define LIQUID_SENSOR_A IO_5V_1
#define LIQUID_SENSOR_B IO_5V_2

// Relay board is active-low: writing 0 energizes the output
#define OUTPUT_ON_LEVEL  0
#define OUTPUT_OFF_LEVEL 1

#endif

#if BOARD_V1

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

// Each fertilizer output drives a full dosing set (pump + valve wired in parallel)
#define FERT_A_GPIO      GPIO_NUM_7
#define HUMIDIFIER_GPIO  GPIO_NUM_6
#define FERT_B_GPIO      GPIO_NUM_5

#define WS2811_CTRL GPIO_NUM_15
#define WS2811_D GPIO_NUM_16

#define LIQUID_SENSOR_1 GPIO_NUM_45
#define LIQUID_SENSOR_2 GPIO_NUM_46

// Standard active-high outputs: writing 1 energizes the output
#define OUTPUT_ON_LEVEL  1
#define OUTPUT_OFF_LEVEL 0

#endif

#endif