#ifndef MAIN_ST7789_H_
#define MAIN_ST7789_H_

#include "driver/spi_master.h"

typedef struct {
	uint16_t _width;
	uint16_t _height;
	uint16_t _offsetx;
	uint16_t _offsety;
	int16_t _dc;
	int16_t _bl;
	spi_device_handle_t _SPIHandle;
} TFT_t;

void spi_clock_speed(int speed);
void spi_master_init(TFT_t * dev, int16_t GPIO_MOSI, int16_t GPIO_SCLK, int16_t GPIO_CS, int16_t GPIO_DC, int16_t GPIO_RESET, int16_t GPIO_BL);
bool spi_master_write_byte(spi_device_handle_t SPIHandle, const uint8_t* Data, size_t DataLength);
bool spi_master_write_command(TFT_t * dev, uint8_t cmd);
bool spi_master_write_data_byte(TFT_t * dev, uint8_t data);
bool spi_master_write_addr(TFT_t * dev, uint16_t addr1, uint16_t addr2);

void delayMS(int ms);
void lcdInit(TFT_t * dev, int width, int height, int offsetx, int offsety);
void lcdInversionOff(TFT_t * dev);
void lcdBacklightOn(TFT_t * dev);
void lcdBacklightOff(TFT_t * dev);

#endif /* MAIN_ST7789_H_ */
