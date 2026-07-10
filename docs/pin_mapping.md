# ESP32-S3 Hydroponic Controller — Pin Mapping

All definitions live in `main/pinout.h`. The sections below group pins by peripheral/function.

---

## ST7789 Display (SPI)

| Signal    | GPIO | Notes                              |
|-----------|------|------------------------------------|
| LCD_SCK   | 11   | SPI clock                          |
| LCD_SDA   | 12   | SPI MOSI (data)                    |
| LCD_CS    | 10   | Chip select (active low)           |
| LCD_WR    | 13   | Data/Command select (RS in schematic) |
| LCD_RESET | 9    | Hardware reset (active low)        |
| LCD_BL    | 4    | Backlight enable                   |

---

## Touch Panel — FT6236 (I2C bus 0)

I2C address: **0x38**

| Signal | GPIO | Notes               |
|--------|------|---------------------|
| TP_SCL | 42   | I2C clock           |
| TP_SDA | 41   | I2C data            |
| TP_INT | 40   | Interrupt (touch detect) |
| TP_RST | 39   | Hardware reset      |

---

## Sensor I2C Bus (bus 1)

Shared by the SHT40 and DYP-A02YYUW sensors.

| Signal         | GPIO | Notes       |
|----------------|------|-------------|
| SENSOR_I2C_SCL | 2    | I2C clock   |
| SENSOR_I2C_SDA | 1    | I2C data    |

### SHT40 — Temperature & Humidity

| Parameter    | Value  |
|--------------|--------|
| Protocol     | I2C    |
| I2C Address  | 0x44   |
| Bus          | Sensor I2C (GPIO 1/2) |

### DYP-A02YYUW — Ultrasonic Distance

| Parameter   | Value  |
|-------------|--------|
| Protocol    | I2C    |
| I2C Address | 0x74   |
| Bus         | Sensor I2C (GPIO 1/2) |
| Output unit | mm     |

---

## BA234 — TDS / EC Sensor (UART)

| Signal          | GPIO | Notes             |
|-----------------|------|-------------------|
| SENSOR_UART_TX  | 14   | ESP32 TX → sensor RX |
| SENSOR_UART_RX  | 21   | ESP32 RX ← sensor TX |

| Parameter  | Value      |
|------------|------------|
| Protocol   | UART       |
| UART port  | UART1      |
| Baud rate  | 9600       |
| Frame      | 8N1        |

---

## Fertilizer Dosing Outputs

Each GPIO drives a pump + valve wired in parallel (relay or MOSFET driver required).

| Signal     | GPIO | Connected to                          |
|------------|------|---------------------------------------|
| FERT_A_GPIO | 7   | Dosing head A — pump + valve (parallel) |
| FERT_B_GPIO | 5   | Dosing head B — pump + valve (parallel) |

---

## Liquid Sensors (Digital Input)

| Signal          | GPIO | Notes                             |
|-----------------|------|-----------------------------------|
| LIQUID_SENSOR_1 | 45   | Line-full detector for head A     |
| LIQUID_SENSOR_2 | 46   | Line-full detector for head B     |

> **Note:** GPIO 45 is also claimed by `RTC_SDA` in pinout.h — a pre-existing conflict. Verify hardware before using both simultaneously.

---

## Grow Light

| Signal       | GPIO | Notes                                    |
|--------------|------|------------------------------------------|
| WS2811_CTRL  | 15   | Grow light relay/driver signal (`GROW_LIGHT_GPIO`) |
| WS2811_D     | 16   | Secondary WS2811 data line (unused in firmware) |

---

## Humidifier

| Signal          | GPIO | Notes                          |
|-----------------|------|--------------------------------|
| HUMIDIFIER_GPIO | 6    | Humidifier relay/driver signal |

> No software control is currently implemented; pin is reserved.

---

## RTC (SPI — reserved)

| Signal  | GPIO | Notes                 |
|---------|------|-----------------------|
| RTC_RST | 18   | Reset                 |
| RTC_CLK | 47   | Clock                 |
| RTC_SDA | 45   | Data (conflicts with LIQUID_SENSOR_1) |

> RTC is defined in pinout.h but not initialized in firmware.

---

## SD Card (SDMMC — reserved)

| Signal | GPIO | Notes      |
|--------|------|------------|
| SD_CLK | 3    | Clock      |
| SD_CMD | 38   | Command    |
| SD_D0  | 8    | Data line  |

> SD card is defined in pinout.h but not initialized in firmware.

---

## Debug

| Signal  | GPIO | Notes      |
|---------|------|------------|
| DBG_LED | 17   | Debug LED  |

---

## Quick Reference — All GPIOs at a Glance

| GPIO | Function              | Direction | Notes                        |
|------|-----------------------|-----------|------------------------------|
| 1    | SENSOR_I2C_SDA        | Bidir     | Sensor I2C data              |
| 2    | SENSOR_I2C_SCL        | Output    | Sensor I2C clock             |
| 3    | SD_CLK                | Output    | SD card clock (unused)       |
| 4    | LCD_BL                | Output    | Display backlight            |
| 5    | FERT_B_GPIO           | Output    | Dosing head B                |
| 6    | HUMIDIFIER_GPIO       | Output    | Humidifier (unused)          |
| 7    | FERT_A_GPIO           | Output    | Dosing head A                |
| 8    | SD_D0                 | Bidir     | SD card data (unused)        |
| 9    | LCD_RESET             | Output    | Display reset                |
| 10   | LCD_CS                | Output    | Display chip select          |
| 11   | LCD_SCK               | Output    | Display SPI clock            |
| 12   | LCD_SDA               | Output    | Display SPI MOSI             |
| 13   | LCD_WR (DC)           | Output    | Display data/command         |
| 14   | SENSOR_UART_TX        | Output    | BA234 TX                     |
| 15   | WS2811_CTRL           | Output    | Grow light control           |
| 16   | WS2811_D              | Output    | WS2811 data (unused)         |
| 17   | DBG_LED               | Output    | Debug LED                    |
| 18   | RTC_RST               | Output    | RTC reset (unused)           |
| 21   | SENSOR_UART_RX        | Input     | BA234 RX                     |
| 38   | SD_CMD                | Bidir     | SD card command (unused)     |
| 39   | TP_RST                | Output    | Touch panel reset            |
| 40   | TP_INT                | Input     | Touch panel interrupt        |
| 41   | TP_SDA                | Bidir     | Touch panel I2C data         |
| 42   | TP_SCL                | Output    | Touch panel I2C clock        |
| 45   | LIQUID_SENSOR_1 / RTC_SDA | Input | **Conflict** — choose one  |
| 46   | LIQUID_SENSOR_2       | Input     | Liquid sensor head B         |
| 47   | RTC_CLK               | Output    | RTC clock (unused)           |
